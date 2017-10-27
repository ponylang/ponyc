#include "lookup.h"
#include "assemble.h"
#include "cap.h"
#include "reify.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include "../ast/id.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../expr/literal.h"
#include "ponyassert.h"
#include <string.h>

static ast_t* lookup_base(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors);

static ast_t* lookup_nominal(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  pony_assert(ast_id(type) == TK_NOMINAL);
  typecheck_t* t = &opt->check;

  ast_t* def = (ast_t*)ast_data(type);
  AST_GET_CHILDREN(def, type_id, typeparams);
  const char* type_name = ast_name(type_id);

  if(is_name_private(type_name) && (from != NULL) && (opt != NULL))
  {
    if(ast_nearest(def, TK_PACKAGE) != t->frame->package)
    {
      if(errors)
      {
        ast_error(opt->check.errors, from, "can't lookup fields or methods "
          "on private types from other packages");
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
      case TK_EMBED:
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // Typecheck default args immediately.
        if(opt != NULL)
        {
          AST_GET_CHILDREN(find, cap, id, typeparams, params);
          ast_t* param = ast_child(params);

          while(param != NULL)
          {
            AST_GET_CHILDREN(param, name, type, def_arg);

            if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL))
            {
              ast_t* child = ast_child(def_arg);

              if(ast_id(child) == TK_CALL)
                ast_settype(child, ast_from(child, TK_INFERTYPE));

              if(ast_visit_scope(&param, pass_pre_expr, pass_expr, opt,
                PASS_EXPR) != AST_OK)
                return NULL;

              def_arg = ast_childidx(param, 2);

              if(!coerce_literals(&def_arg, type, opt))
                return NULL;
            }

            param = ast_sibling(param);
          }
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
      ast_error(opt->check.errors, from, "couldn't find '%s' in '%s'", name,
        type_name);

    return NULL;
  }

  if(is_name_private(name) && (from != NULL) && (opt != NULL))
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        if(t->frame->type != def)
        {
          if(errors)
          {
            ast_error(opt->check.errors, from,
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
            ast_error(opt->check.errors, from,
              "can't lookup private methods from outside the package");
          }

          return NULL;
        }
        break;
      }

      default:
        pony_assert(0);
        return NULL;
    }

    if(name == stringtab("_final"))
    {
      switch(ast_id(find))
      {
        case TK_NEW:
        case TK_BE:
        case TK_FUN:
          if(errors)
            ast_error(opt->check.errors, from,
              "can't lookup a _final function");

          return NULL;

        default: {}
      }
    } else if((name == stringtab("_init")) && (ast_id(def) == TK_PRIMITIVE)) {
      switch(ast_id(find))
      {
        case TK_NEW:
        case TK_BE:
        case TK_FUN:
          break;

        default:
          pony_assert(0);
      }

      if(errors)
        ast_error(opt->check.errors, from,
          "can't lookup an _init function on a primitive");

      return NULL;
    }
  }

  ast_t* typeargs = ast_childidx(type, 2);
  ast_t* r_find = viewpoint_replacethis(find, orig);
  return reify(r_find, typeparams, typeargs, opt, false);
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
      ast_error(opt->check.errors, from, "couldn't find '%s' in '%s'",
        name, type_name);
    }

    return NULL;
  }

  // Lookup on the constraint instead.
  return lookup_base(opt, from, orig, constraint, name, errors);
}

static bool param_names_match(ast_t* from, ast_t* prev_fun, ast_t* cur_fun,
  const char* name, errorframe_t* pframe, pass_opt_t* opt)
{
  ast_t* parent = from != NULL ? ast_parent(from) : NULL;
  if(parent != NULL && ast_id(parent) == TK_CALL)
  {
    AST_GET_CHILDREN(parent, receiver, positional, namedargs);
    if(namedargs != NULL && ast_id(namedargs) == TK_NAMEDARGS)
    {
      AST_GET_CHILDREN(prev_fun, prev_cap, prev_name, prev_tparams,
        prev_params);
      AST_GET_CHILDREN(cur_fun, cur_cap, cur_name, cur_tparams, cur_params);

      ast_t* prev_param = ast_child(prev_params);
      ast_t* cur_param = ast_child(cur_params);

      while(prev_param != NULL && cur_param != NULL)
      {
        AST_GET_CHILDREN(prev_param, prev_id);
        AST_GET_CHILDREN(cur_param, cur_id);

        if(ast_name(prev_id) != ast_name(cur_id))
        {
          if(pframe != NULL)
          {
            errorframe_t frame = NULL;
            ast_error_frame(&frame, from, "the '%s' methods of this union"
              " type have different parameter names; this prevents their"
              " use as named arguments", name);
            errorframe_append(&frame, pframe);
            errorframe_report(&frame, opt->check.errors);
          }
          return false;
        }

        prev_param = ast_sibling(prev_param);
        cur_param = ast_sibling(cur_param);
      }
    }
  }

  return true;
}

static ast_t* lookup_union(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool errors)
{
  ast_t* child = ast_child(type);
  ast_t* result = NULL;
  bool ok = true;

  while(child != NULL)
  {
    ast_t* r = lookup_base(opt, from, child, child, name, errors);

    if(r == NULL)
    {
      // All possible types in the union must have this.
      if(errors)
      {
        ast_error(opt->check.errors, from, "couldn't find %s in %s",
          name, ast_print_type(child));
      }

      ok = false;
    } else {
      switch(ast_id(r))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup field %s in %s in a union type",
              name, ast_print_type(child));
          }

          ok = false;
          break;

        default:
        {
          errorframe_t frame = NULL;
          errorframe_t* pframe = errors ? &frame : NULL;

          if(result == NULL)
          {
            // If we don't have a result yet, use this one.
            result = r;
          } else if(!is_subtype_fun(r, result, pframe, opt)) {
            if(is_subtype_fun(result, r, pframe, opt))
            {
              // Use the supertype function. Require the most specific
              // arguments and return the least specific result.
              if(!param_names_match(from, result, r, name, pframe, opt))
              {
                ast_free_unattached(r);
                ok = false;
              } else {
                if(errors)
                  errorframe_discard(pframe);

                ast_free_unattached(result);
                result = r;
              }
            } else {
              if(errors)
              {
                errorframe_t frame = NULL;
                ast_error_frame(&frame, from,
                  "a member of the union type has an incompatible method "
                  "signature");
                errorframe_append(&frame, pframe);
                errorframe_report(&frame, opt->check.errors);
              }

              ast_free_unattached(r);
              ok = false;
            }
          } else {
            if(!param_names_match(from, result, r, name, pframe, opt))
            {
              ast_free_unattached(r);
              ok = false;
            }
          }
          break;
        }
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

static ast_t* lookup_isect(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool errors)
{
  ast_t* child = ast_child(type);
  ast_t* result = NULL;
  bool ok = true;

  while(child != NULL)
  {
    ast_t* r = lookup_base(opt, from, child, child, name, false);

    if(r != NULL)
    {
      switch(ast_id(r))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          // Ignore fields.
          break;

        default:
          if(result == NULL)
          {
            // If we don't have a result yet, use this one.
            result = r;
          } else if(!is_subtype_fun(result, r, NULL, opt)) {
            if(is_subtype_fun(r, result, NULL, opt))
            {
              // Use the subtype function. Require the least specific
              // arguments and return the most specific result.
              ast_free_unattached(result);
              result = r;
            }

            // TODO: isect the signatures, to handle arg names and
            // default arguments. This is done even when the functions have
            // no subtype relationship.
          }
          break;
      }
    }

    child = ast_sibling(child);
  }

  if(errors && (result == NULL))
    ast_error(opt->check.errors, from, "couldn't find '%s'", name);

  if(!ok)
  {
    ast_free_unattached(result);
    result = NULL;
  }

  return result;
}

static ast_t* lookup_base(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return lookup_union(opt, from, type, name, errors);

    case TK_ISECTTYPE:
      return lookup_isect(opt, from, type, name, errors);

    case TK_TUPLETYPE:
      if(errors)
        ast_error(opt->check.errors, from, "can't lookup by name on a tuple");

      return NULL;

    case TK_DONTCARETYPE:
      if(errors)
        ast_error(opt->check.errors, from, "can't lookup by name on '_'");

      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(opt, from, orig, type, name, errors);

    case TK_ARROW:
      return lookup_base(opt, from, orig, ast_childidx(type, 1), name, errors);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(opt, from, orig, type, name, errors);

    case TK_FUNTYPE:
      if(errors)
        ast_error(opt->check.errors, from,
          "can't lookup by name on a function type");

      return NULL;

    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      // Can only happen due to a local inference fail earlier
      return NULL;

    default: {}
  }

  pony_assert(0);
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
