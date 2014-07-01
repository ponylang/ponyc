#include "lookup.h"
#include "nominal.h"
#include "reify.h"
#include <assert.h>

static ast_t* lookup_nominal(ast_t* scope, ast_t* type, const char* name)
{
  assert(ast_id(type) == TK_NOMINAL);

  if(!nominal_valid(scope, type))
    return NULL;

  ast_t* def = nominal_def(scope, type);
  ast_t* typename = ast_child(def);
  ast_t* typeparams = ast_sibling(typename);
  ast_t* typeargs = ast_childidx(type, 2);
  ast_t* find;

  if(ast_id(def) == TK_TYPE)
  {
    ast_t* alias = ast_childidx(def, 3);

    if(ast_id(alias) == TK_NONE)
      return NULL;

    alias = ast_child(alias);
    ast_t* r_alias = reify(alias, typeparams, typeargs);
    find = lookup(scope, r_alias, name);
    ast_free_unattached(r_alias);
  } else {
    find = ast_get(def, name);
  }

  // TODO: reify again here?
  if(find == NULL)
    ast_error(type, "couldn't find '%s' in '%s'", name, ast_name(typename));

  return find;
}

static ast_t* lookup_structural(ast_t* scope, ast_t* type, const char* name)
{
  assert(ast_id(type) == TK_STRUCTURAL);
  ast_t* find = ast_get(type, name);

  if(find == NULL)
    ast_error(type, "couldn't find '%s'", name);

  return find;
}

ast_t* lookup(ast_t* scope, ast_t* type, const char* name)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      ast_error(type, "can't lookup by name on a union type");
      return NULL;

    case TK_ISECTTYPE:
      ast_error(type, "can't lookup by name on an intersection type");
      return NULL;

    case TK_TUPLETYPE:
      ast_error(type, "can't lookup by name on a tuple");
      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(scope, type, name);

    case TK_STRUCTURAL:
      return lookup_structural(scope, type, name);

    case TK_ARROW:
      return lookup(scope, ast_childidx(type, 1), name);

    case TK_ERROR:
      ast_error(type, "can't lookup by name on an error");
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}
