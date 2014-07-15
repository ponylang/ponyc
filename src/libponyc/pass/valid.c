#include "valid.h"
#include "../type/nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool valid_nominal(ast_t* ast)
{
  if(!nominal_valid(ast, &ast))
    return false;

  if(ast_id(ast) != TK_NOMINAL)
    return true;

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
      ast_t* def = nominal_def(ast, left);
      assert(def != NULL);

      if(ast_id(def) == TK_TYPEPARAM)
        return true;

      break;
    }

    default: {}
  }

  ast_error(left, "only type parameters and 'this' can be viewpoints");
  return false;
}

ast_result_t pass_valid(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!valid_nominal(ast))
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
