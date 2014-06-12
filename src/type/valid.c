#include "valid.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool check_constraints(ast_t* type, ast_t* typeargs)
{
  if(ast_id(type) == TK_TYPEPARAM)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type parameters cannot have type arguments");
      return false;
    }

    return true;
  }

  // reify the type parameters with the typeargs
  ast_t* typeparams = ast_childidx(type, 1);
  ast_t* r_typeparams = reify(typeparams, typeparams, typeargs);

  if(r_typeparams == NULL)
    return false;

  ast_t* typeparam = ast_child(r_typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(typeparam, 1);

    if((ast_id(constraint) != TK_NONE) && !is_subtype(typearg, constraint))
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");

      if(r_typeparams != typeparams)
        ast_free(r_typeparams);

      return false;
    }

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(r_typeparams != typeparams)
    ast_free(r_typeparams);

  return true;
}

static bool replace_alias(ast_t* def, ast_t* nominal, ast_t* typeargs)
{
  // see if this is a type alias
  if(ast_id(def) != TK_TYPE)
    return true;

  ast_t* alias_list = ast_childidx(def, 3);

  // if this type alias has no alias list, we're done
  // so type None stays as None
  // but type Bool is (True | False) must be replaced with (True | False)
  if(ast_id(alias_list) == TK_NONE)
    return true;

  // we should have a single TYPEDEF as our alias
  assert(ast_id(alias_list) == TK_TYPES);
  ast_t* alias = ast_child(alias_list);
  assert(ast_sibling(alias) == NULL);

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* r_alias = reify(alias, typeparams, typeargs);

  if(r_alias == NULL)
    return false;

  if(r_alias == alias)
    r_alias = ast_dup(alias);

  // the thing being swapped in must be a TYPEDEF
  assert(ast_id(r_alias) == TK_TYPEDEF);

  // the thing being swapped out must be a TYPEDEF
  ast_t* type_def = ast_parent(nominal);
  assert(ast_id(type_def) == TK_TYPEDEF);

  // use the new ast and free the old one
  ast_swap(type_def, r_alias);
  ast_free(type_def);

  return true;
}

/**
 * This checks that all explicit types are valid. It also attaches the defintion
 * ast node to each nominal type, and replaces type aliases with their reified
 * expansion.
 */
bool type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      // TODO: capability
      ast_t* def = nominal_def(ast);

      if(def == NULL)
        return false;

      // make sure our typeargs are subtypes of our constraints
      ast_t* typeargs = ast_childidx(ast, 2);

      if(!check_constraints(def, typeargs))
        return false;

      if(!replace_alias(def, ast, typeargs))
        return false;

      return true;
    }

    default: {}
  }

  return true;
}
