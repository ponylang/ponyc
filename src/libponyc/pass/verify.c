#include "verify.h"
#include "../verify/call.h"
#include "../verify/control.h"
#include "../verify/fun.h"
#include <assert.h>

ast_result_t pass_verify(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_FUN:
    case TK_NEW:
    case TK_BE:           r = verify_fun(options, ast); break;
    case TK_FUNREF:
    case TK_FUNCHAIN:
    case TK_NEWREF:       r = verify_function_call(options, ast); break;
    case TK_BEREF:
    case TK_BECHAIN:
    case TK_NEWBEREF:     r = verify_behaviour_call(options, ast); break;
    case TK_FFICALL:      r = verify_ffi_call(options, ast); break;
    case TK_TRY:
    case TK_TRY_NO_CHECK: r = verify_try(options, ast); break;
    case TK_LETREF:
    case TK_VARREF:
    case TK_FLETREF:
    case TK_FVARREF:
    case TK_EMBEDREF:
    case TK_CALL:
    case TK_QUALIFY:
    case TK_TUPLE:
    case TK_ASSIGN:
    case TK_MATCH:
    case TK_CASES:
    case TK_CASE:
    case TK_IS:
    case TK_ISNT:
    case TK_SEQ:
    case TK_BREAK:
    case TK_RETURN:
    case TK_IF:
    case TK_IFDEF:
    case TK_WHILE:
    case TK_REPEAT:
    case TK_RECOVER:
    case TK_POSITIONALARGS:
    case TK_NAMEDARGS:
    case TK_NAMEDARG:
    case TK_UPDATEARG:    ast_inheritflags(ast); break;
    case TK_ERROR:        ast_seterror(ast); break;

    default: {}
  }

  if(!r)
  {
    assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}
