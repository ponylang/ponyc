#include "lookup.h"
#include "assemble.h"
#include "reify.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include <string.h>
#include <assert.h>

static ast_t* lookup_base(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors);

static ast_t* lookup_nominal(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  assert(ast_id(type) == TK_NOMINAL);
  typecheck_t* t = &opt->check;

  ast_t* def = (ast_t*)ast_data(type);
  AST_GET_CHILDREN(def, type_id, typeparams);
  const char* type_name = ast_name(type_id);

  if((type_name[0] == '_') && (from != NULL) && (t != NULL))
  {
    if(ast_nearest(def, TK_PACKAGE) != t->frame->package)
    {
      if(errors)
      {
        ast_error(from,
          "can't lookup fields or methods on private types from other packages"
          );
      }

      return NULL;
    }
  }

  ast_t* find = ast_get(def, name, NULL);

  if(find != NULL)
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // Typecheck default args immediately.
        AST_GET_CHILDREN(find, cap, id, typeparams, params);
        ast_t* param = ast_child(params);

        while(param != NULL)
        {
          AST_GET_CHILDREN(param, name, type, def_arg);

          if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL))
          {
            ast_settype(def_arg, ast_from(def_arg, TK_INFERTYPE));

            if(ast_visit(&def_arg, NULL, pass_expr, opt) != AST_OK)
              return false;

            ast_visit(&def_arg, NULL, pass_nodebug, opt);
          }

          param = ast_sibling(param);
        }
        break;
      }

      default:
        find = NULL;
    }
  }

  if(find == NULL)
  {
    if(errors)
      ast_error(from, "couldn't find '%s' in '%s'", name, type_name);

    return NULL;
  }

  if((name[0] == '_') && (from != NULL) && (t != NULL))
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
        if(t->frame->type != def)
        {
          if(errors)
          {
            ast_error(from,
              "can't lookup private fields from outside the type");
          }

          return NULL;
        }
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        if(ast_nearest(def, TK_PACKAGE) != t->frame->package)
        {
          if(errors)
          {
            ast_error(from,
              "can't lookup private methods from outside the package");
          }

          return NULL;
        }
        break;
      }

      default:
        assert(0);
        return NULL;
    }

    if(!strcmp(name, "_final"))
    {
      switch(ast_id(find))
      {
        case TK_NEW:
        case TK_BE:
        case TK_FUN:
          if(errors)
            ast_error(from, "can't lookup a _final function");

          return NULL;

        default: {}
      }
    }
  }

  ast_t* typeargs = ast_childidx(type, 2);

  find = ast_dup(find);
  orig = ast_dup(orig);
  replace_thistype(&find, orig);
  ast_free_unattached(orig);

  ast_t* r_find = reify(from, find, typeparams, typeargs);

  if(r_find != find)
  {
    ast_free_unattached(find);
    find = r_find;
  }

  if((find != NULL) && !flatten_arrows(&find, errors))
  {
    if(errors)
      ast_error(from, "can't look this up on a tag");

    ast_free_unattached(find);
    return NULL;
  }

  return find;
}

static ast_t* lookup_typeparam(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* constraint = ast_childidx(def, 1);
  ast_t* constraint_def = (ast_t*)ast_data(constraint);

  if(def == constraint_def)
  {
    if(errors)
    {
      ast_t* type_id = ast_child(def);
      const char* type_name = ast_name(type_id);
      ast_error(from, "couldn't find '%s' in '%s'", name, type_name);
    }

    return NULL;
  }

  // Lookup on the constraint instead.
  return lookup_base(opt, from, orig, constraint, name, errors);
}

static ast_t* lookup_base(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(type);
      ast_t* result = NULL;
      bool ok = true;

      while(child != NULL)
      {
        ast_t* r = lookup_base(opt, from, orig, child, name, errors);

        if(r == NULL)
        {
          if(errors)
          {
            ast_error(from, "couldn't find %s in %s",
              name, ast_print_type(child));
          }

          ok = false;
        } else {
          switch(ast_id(r))
          {
            case TK_FVAR:
            case TK_FLET:
              if(errors)
              {
                ast_error(from,
                  "can't lookup field %s in %s in a union type",
                  name, ast_print_type(child));
              }

              ok = false;
              break;

            default:
              if(result == NULL)
              {
                result = r;
              } else if(!is_subtype(r, result)) {
                if(is_subtype(result, r))
                {
                  ast_free_unattached(result);
                  result = r;
                } else {
                  if(errors)
                  {
                    ast_error(from,
                      "a member of the union type has an incompatible method "
                      "signature");
                    ast_error(result, "first implementation is here");
                    ast_error(r, "second implementation is here");
                  }

                  ast_free_unattached(r);
                  ok = false;
                }
              }
              break;
          }
        }

        child = ast_sibling(child);
      }

      if(!ok)
      {
        ast_free_unattached(result);
        result = NULL;
      }

      return result;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);
      ast_t* result = NULL;
      bool ok = true;

      while(child != NULL)
      {
        ast_t* r = lookup_base(opt, from, orig, child, name, false);

        if(r != NULL)
        {
          switch(ast_id(r))
          {
            case TK_FVAR:
            case TK_FLET:
              if(errors)
              {
                ast_error(from,
                  "can't lookup field %s in %s in an intersection type",
                  name, ast_print_type(child));
              }

              ok = false;
              break;

            default:
              if(result == NULL)
              {
                result = r;
              } else if(!is_subtype(result, r)) {
                if(is_subtype(r, result))
                {
                  ast_free_unattached(result);
                  result = r;
                } else {
                  if(errors)
                  {
                    ast_error(from,
                      "a member of the intersection type has an incompatible "
                      "method signature");
                    ast_error(result, "first implementation is here");
                    ast_error(r, "second implementation is here");
                  }

                  ast_free_unattached(r);
                  ok = false;
                }
              }
              break;
          }
        }

        child = ast_sibling(child);
      }

      if(errors && (result == NULL))
        ast_error(from, "couldn't find '%s'", name);

      if(!ok)
      {
        ast_free_unattached(result);
        result = NULL;
      }

      return result;
    }

    case TK_TUPLETYPE:
      if(errors)
        ast_error(from, "can't lookup by name on a tuple");

      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(opt, from, orig, type, name, errors);

    case TK_ARROW:
      return lookup_base(opt, from, orig, ast_childidx(type, 1), name, errors);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(opt, from, orig, type, name, errors);

    case TK_FUNTYPE:
      if(errors)
        ast_error(from, "can't lookup by name on a function type");

      return NULL;

    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      // Can only happen due to a local inference fail earlier
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* lookup(pass_opt_t* opt, ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(opt, from, type, type, name, true);
}

ast_t* lookup_try(pass_opt_t* opt, ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(opt, from, type, type, name, false);
}
