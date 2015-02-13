#include "lookup.h"
#include "assemble.h"
#include "reify.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include <stdio.h>
#include <assert.h>

static ast_t* lookup_base(typecheck_t* t, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors);

static ast_t* lookup_nominal(typecheck_t* t, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  assert(ast_id(type) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* type_name = ast_child(def);
  ast_t* find = ast_get(def, name, NULL);

  if(find != NULL)
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
        break;

      default:
        find = NULL;
    }
  }

  if(find == NULL)
  {
    if(errors)
      ast_error(from, "couldn't find '%s' in '%s'", name, ast_name(type_name));

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
  }

  ast_t* typeparams = ast_sibling(type_name);
  ast_t* typeargs = ast_childidx(type, 2);

  find = ast_dup(find);
  replace_thistype(&find, orig);

  ast_t* r_find = reify(find, typeparams, typeargs);

  if(r_find != find)
  {
    ast_free_unattached(find);
    find = r_find;
  }

  if(!flatten_arrows(&find, errors))
  {
    if(errors)
      ast_error(from, "can't look this up on a tag");

    ast_free_unattached(find);
    return NULL;
  }

  return find;
}

static ast_t* lookup_typeparam(typecheck_t* t, ast_t* from, ast_t* orig,
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
  return lookup_base(t, from, orig, constraint, name, errors);
}

static ast_t* lookup_base(typecheck_t* t, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      if(errors)
        ast_error(from, "can't lookup by name on a union type");

      return NULL;

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* result = lookup_base(t, from, orig, child, name, false);

        if(result != NULL)
          return result;

        child = ast_sibling(child);
      }

      ast_error(from, "couldn't find '%s'", name);
      return NULL;
    }

    case TK_TUPLETYPE:
      if(errors)
        ast_error(from, "can't lookup by name on a tuple");

      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(t, from, orig, type, name, errors);

    case TK_ARROW:
      return lookup_base(t, from, orig, ast_childidx(type, 1), name, errors);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(t, from, orig, type, name, errors);

    case TK_FUNTYPE:
      if(errors)
        ast_error(from, "can't lookup by name on a function type");

      return NULL;

    case TK_INFERTYPE:
      // Can only happen due to a local inference fail earlier
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* lookup(typecheck_t* t, ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(t, from, type, type, name, true);
}

ast_t* lookup_try(typecheck_t* t, ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(t, from, type, type, name, false);
}
