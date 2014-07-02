#include "valid.h"
#include "../type/nominal.h"
#include <assert.h>

static bool valid_nominal(ast_t* ast)
{
  if(!nominal_valid(ast, ast))
    return false;

  ast_t* ephemeral = ast_childidx(ast, 4);

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

  ast_replace(cap, ast_from(ast, def_cap));
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
ast_result_t pass_valid(ast_t* ast, int verbose)
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
