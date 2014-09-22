#include "lookup.h"
#include "assemble.h"
#include "reify.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include <assert.h>

static ast_t* lookup_base(ast_t* from, ast_t* orig, ast_t* type,
  const char* name);

static ast_t* lookup_nominal(ast_t* from, ast_t* orig, ast_t* type,
  const char* name)
{
  assert(ast_id(type) == TK_NOMINAL);
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* type_name = ast_child(def);
  ast_t* find = ast_get(def, name, NULL);

  if(find != NULL)
  {
    if((name[0] == '_') && (from != NULL))
    {
      switch(ast_id(find))
      {
        case TK_FVAR:
        case TK_FLET:
          if(ast_enclosing_type(from) != def)
          {
            ast_error(from,
              "can't lookup private fields from outside the type");
            return NULL;
          }
          break;

        case TK_NEW:
        case TK_BE:
        case TK_FUN:
        {
          ast_t* package1 = ast_nearest(def, TK_PACKAGE);
          ast_t* package2 = ast_nearest(from, TK_PACKAGE);

          if(package1 != package2)
          {
            ast_error(from,
              "can't lookup private methods from outside the package");
            return NULL;
          }
          break;
        }

        default:
          assert(0);
          return NULL;
      }
    }

    find = ast_dup(find);
    flatten_thistype(&find, orig);

    ast_t* typeparams = ast_sibling(type_name);
    ast_t* typeargs = ast_childidx(type, 2);
    find = reify(find, typeparams, typeargs);
  } else {
    ast_error(from, "couldn't find '%s' in '%s'", name, ast_name(type_name));
  }

  return find;
}

static ast_t* lookup_structural(ast_t* from, ast_t* type, const char* name)
{
  assert(ast_id(type) == TK_STRUCTURAL);

  ast_t* find = ast_get(type, name, NULL);

  if(find == NULL)
    ast_error(from, "couldn't find '%s'", name);

  if(name[0] == '_')
  {
    ast_error(from, "can't lookup a private name on a structural type");
    return NULL;
  }

  return find;
}

static ast_t* lookup_typeparam(ast_t* from, ast_t* orig, ast_t* type,
  const char* name)
{
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* constraint = ast_childidx(def, 1);

  // lookup on the constraint instead
  return lookup_base(from, orig, constraint, name);
}

static ast_t* lookup_base(ast_t* from, ast_t* orig, ast_t* type,
  const char* name)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      ast_error(from, "can't lookup by name on a union type");
      return NULL;

    case TK_ISECTTYPE:
      ast_error(from, "can't lookup by name on an intersection type");
      return NULL;

    case TK_TUPLETYPE:
      ast_error(from, "can't lookup by name on a tuple");
      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(from, orig, type, name);

    case TK_STRUCTURAL:
      return lookup_structural(from, type, name);

    case TK_ARROW:
      return lookup_base(from, orig, ast_childidx(type, 1), name);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(from, orig, type, name);

    case TK_FUNTYPE:
      ast_error(from, "can't lookup by name on a function type");
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* lookup(ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(from, type, type, name);
}
