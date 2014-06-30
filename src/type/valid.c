#include "valid.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool is_typeparam(ast_t* scope, ast_t* typeparam, ast_t* typearg)
{
  if(ast_id(typearg) != TK_NOMINAL)
    return false;

  ast_t* def = nominal_def(scope, typearg);
  return def == typeparam;
}

static bool check_constraints(ast_t* scope, ast_t* def, ast_t* typeargs)
{
  if(ast_id(def) == TK_TYPEPARAM)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type parameters cannot have type arguments");
      return false;
    }

    return true;
  }

  // reify the type parameters with the typeargs
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* r_typeparams = reify(typeparams, typeparams, typeargs);

  if(r_typeparams == NULL)
    return false;

  ast_t* r_typeparam = ast_child(r_typeparams);
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((r_typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(r_typeparam, 1);

    // compare the typearg to the typeparam and constraint
    if(!is_typeparam(scope, typeparam, typearg) &&
      (ast_id(constraint) != TK_NONE) &&
      !is_subtype(scope, typearg, constraint)
      )
    {
      // TODO: remove this
      is_subtype(scope, typearg, constraint);

      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");
      ast_free_unattached(r_typeparams);
      return false;
    }

    r_typeparam = ast_sibling(r_typeparam);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  ast_free_unattached(r_typeparams);
  return true;
}

static bool check_alias_cap(ast_t* def, ast_t* cap)
{
  ast_t* alias = ast_child(ast_childidx(def, 3));

  if(alias == NULL)
  {
    // no cap on singleton types
    if(ast_id(cap) != TK_NONE)
    {
      ast_error(cap, "can't specify a capability on a marker type");
      return false;
    }

    return true;
  }

  switch(ast_id(alias))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // no cap allowed
      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap,
          "can't specify a capability on an alias to a union, isect or "
          " tuple type");
        return false;
      }

      return true;
    }

    case TK_NOMINAL:
    {
      // TODO: does the alias specify a cap?
      // if so... ?
      return true;
    }

    case TK_STRUCTURAL:
    {
      // TODO: does the alias specify a cap?
      // if so... ?
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool check_cap(ast_t* def, ast_t* cap)
{
  switch(ast_id(def))
  {
    case TK_TYPEPARAM:
      return true;

    case TK_TYPE:
      return check_alias_cap(def, cap);

    case TK_TRAIT:
      return true;

    case TK_CLASS:
      return true;

    case TK_ACTOR:
      return true;

    default: {}
  }

  assert(0);
  return false;
}

static bool valid_nominal(ast_t* ast)
{
  ast_t* def = nominal_def(ast, ast);

  if(def == NULL)
    return false;

  // make sure our typeargs are subtypes of our constraints
  ast_t* typeargs = ast_childidx(ast, 2);
  ast_t* cap = ast_sibling(typeargs);
  ast_t* ephemeral = ast_sibling(cap);

  if(!check_constraints(ast, def, typeargs))
    return false;

  if(!check_cap(def, cap))
    return false;

  if((ast_id(ephemeral) == TK_HAT) && (ast_enclosing_method_type(ast) == NULL))
  {
    ast_error(ephemeral,
      "ephemeral types can only appear in function return types");
    return false;
  }

  return true;
}

static bool valid_structural(ast_t* ast)
{
  ast_t* cap = ast_childidx(ast, 1);
  ast_t* ephemeral = ast_sibling(cap);

  if((ast_id(ephemeral) == TK_HAT) && (ast_enclosing_method_type(ast) == NULL))
  {
    ast_error(ephemeral,
      "ephemeral types can only appear in function return types");
    return false;
  }

  if(ast_id(cap) != TK_NONE)
    return true;

  token_id def_cap;

  // if it's a typeparam, default capability is tag, otherwise it is ref
  if(ast_nearest(ast, TK_TYPEPARAM) != NULL)
    def_cap = TK_TAG;
  else
    def_cap = TK_REF;

  ast_swap(cap, ast_from(ast, def_cap));
  return true;
}

static bool valid_thistype(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  if(ast_id(parent) != TK_ARROW)
  {
    ast_error(ast, "in a type, 'this' can only be used as a viewpoint");
    return false;
  }

  if((ast_id(ast_parent(parent)) == TK_ARROW) || (ast_child(parent) != ast))
  {
    ast_error(ast, "when using 'this' for viewpoint it must come first");
    return false;
  }

  if(ast_enclosing_method(ast) == NULL)
  {
    ast_error(ast, "can only use 'this' for a viewpoint in a method");
    return false;
  }

  return true;
}

static bool valid_arrow(ast_t* ast)
{
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_THISTYPE:
      return true;

    case TK_NOMINAL:
    {
      // left side will already have been validated
      ast_t* def = nominal_def(ast, left);

      if(ast_id(def) == TK_TYPEPARAM)
        return true;

      break;
    }

    default: {}
  }

  ast_error(left, "only type parameters and 'this' can be viewpoints");
  return false;
}

/**
 * This checks that all explicit types are valid. It also attaches the
 * definition ast node to each nominal type, and replaces type aliases with
 * their reified expansion.
 */
ast_result_t type_valid(ast_t* ast, int verbose)
{
  // union, isect and tuple types don't need validation
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!valid_nominal(ast))
        return AST_ERROR;
      break;

    case TK_STRUCTURAL:
      if(!valid_structural(ast))
        return AST_ERROR;
      break;

    case TK_THISTYPE:
      if(!valid_thistype(ast))
        return AST_ERROR;
      break;

    case TK_ARROW:
      if(!valid_arrow(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
