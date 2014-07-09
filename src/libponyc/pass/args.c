#include "args.h"
#include "../type/subtype.h"
#include <assert.h>

static bool args_call(ast_t* ast)
{
  // args are checked after expressions because parameters with default values
  // may not have their types inferred until after expression type checking.
  ast_t* left = ast_child(ast);
  ast_t* positional = ast_sibling(left);
  ast_t* named = ast_sibling(positional);
  ast_t* type = ast_type(left);
  assert(ast_id(type) == TK_FUNTYPE);

  // TODO: named arguments
  if(ast_id(named) != TK_NONE)
  {
    ast_error(named, "named arguments not yet supported");
    return false;
  }

  // check positional args vs params
  ast_t* typeparams = ast_child(type);
  assert(ast_id(typeparams) == TK_NONE);

  // TODO: is this wrong?
  ast_t* params = ast_sibling(typeparams);
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while((arg != NULL) && (param != NULL))
  {
    if(ast_id(param) == TK_NONE)
    {
      ast_error(arg, "undecided default argument (not implemented)");
      return false;
    }

    if(!is_subtype(ast, ast_type(arg), param))
    {
      ast_error(arg, "argument not a subtype of parameter");
      return false;
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  if(arg != NULL)
  {
    ast_error(arg, "too many arguments");
    return false;
  }

  if(param != NULL)
  {
    ast_error(ast, "not enough arguments");
    return false;
  }

  return true;
}

ast_result_t pass_args(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_CALL:
      if(!args_call(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
