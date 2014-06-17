#include "valid.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool is_typeparam(ast_t* typeparam, ast_t* typearg)
{
  assert(ast_id(typearg) == TK_TYPEDEF);
  ast_t* sub = ast_child(typearg);

  if(ast_id(sub) != TK_NOMINAL)
    return false;

  ast_t* def = nominal_def(typearg, sub);
  return def == typeparam;
}

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

  // dangle r_typeparams from the parent of typeparams in order to resolve in
  // the same scope.
  ast_t* r_typeparams = reify(typeparams, typeparams, typeargs);
  ast_dangle(r_typeparams, ast_parent(typeparams));

  if(r_typeparams == NULL)
    return false;

  ast_t* r_typeparam = ast_child(r_typeparams);
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((r_typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(r_typeparam, 1);

    // compare the typearg to the typeparam and constraint
    if(!is_typeparam(typeparam, typearg) &&
      (ast_id(constraint) != TK_NONE) &&
      !is_subtype(type, typearg, constraint)
      )
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");

      if(r_typeparams != typeparams)
        ast_free(r_typeparams);

      return false;
    }

    r_typeparam = ast_sibling(r_typeparam);
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

  // extract from the typedef
  assert(ast_id(r_alias) == TK_TYPEDEF);
  ast_t* alias_def = ast_child(r_alias);

  // use the alias definition and free the previous nominal type
  ast_swap(nominal, alias_def);
  ast_free(nominal);
  ast_free_unattached(r_alias);

  return true;
}

/**
 * This checks that all explicit types are valid. It also attaches the
 * definition ast node to each nominal type, and replaces type aliases with
 * their reified expansion.
 */
ast_result_t type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!valid_nominal(ast, ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

bool valid_nominal(ast_t* scope, ast_t* nominal)
{
  // TODO: capability
  ast_t* def = nominal_def(scope, nominal);

  if(def == NULL)
    return false;

  // make sure our typeargs are subtypes of our constraints
  ast_t* typeargs = ast_childidx(nominal, 2);

  if(!check_constraints(def, typeargs))
    return false;

  if(!replace_alias(def, nominal, typeargs))
    return false;

  return true;
}
