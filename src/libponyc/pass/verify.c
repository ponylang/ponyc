#include "verify.h"
#include "../verify/fun.h"
#include <assert.h>

ast_result_t pass_verify(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN: r = verify_fun(options, ast); break;

    default: {}
  }

  if(!r)
  {
    assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}
