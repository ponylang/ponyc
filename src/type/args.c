#include "args.h"
#include "subtype.h"
#include <assert.h>

static bool args_call(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* positional = ast_sibling(left);
  ast_t* named = ast_sibling(positional);
  ast_t* type = ast_type(left);

  if(ast_id(named) != TK_NONE)
  {
    ast_error(named, "named arguments not yet supported");
    return false;
  }

  switch(ast_id(type))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      // method call on 'this'
      // check positional args vs params
      ast_t* params = ast_childidx(type, 3);
      ast_t* param = ast_child(params);
      ast_t* arg = ast_child(positional);

      while((arg != NULL) && (param != NULL))
      {
        if(!is_subtype(ast_type(arg), ast_type(param)))
        {
          ast_error(arg, "argument not a subtype of parameter");
          ast_error(param, "parameter is here");
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

      // TODO: named arguments
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

ast_result_t type_args(ast_t* ast, int verbose)
{
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
