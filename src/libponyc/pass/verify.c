#include "verify.h"
#include "../verify/call.h"
#include "../verify/control.h"
#include "../verify/fun.h"
#include "../verify/type.h"
#include "expr.h"
#include "ponyassert.h"


static bool verify_assign_lvalue(pass_opt_t* opt, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_FLETREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(opt->check.errors, ast, "can't reassign to a let field");
        return false;
      }

      if(opt->check.frame->loop_body != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let field in a loop");
        return false;
      }

      break;
    }

    case TK_EMBEDREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(opt->check.errors, ast, "can't assign to an embed field");
        return false;
      }

      if(opt->check.frame->loop_body != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to an embed field in a loop");
        return false;
      }

      break;
    }

    case TK_TUPLEELEMREF:
    {
      ast_error(opt->check.errors, ast,
        "can't assign to an element of a tuple");
      return false;
    }

    case TK_TUPLE:
    {
      // Verify every lvalue in the tuple.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!verify_assign_lvalue(opt, child))
          return false;

        child = ast_sibling(child);
      }

      break;
    }

    case TK_SEQ:
    {
      // This is used because the elements of a tuple are sequences,
      // each sequence containing a single child.
      ast_t* child = ast_child(ast);
      pony_assert(ast_sibling(child) == NULL);

      return verify_assign_lvalue(opt, child);
    }

    default: {}
  }

  return true;
}


static bool verify_assign(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_ASSIGN);
  AST_GET_CHILDREN(ast, left, right);

  if(!verify_assign_lvalue(opt, left))
    return false;

  ast_inheritflags(ast);

  return true;
}


ast_result_t pass_verify(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_STRUCT:       r = verify_struct(options, ast); break;
    case TK_ASSIGN:       r = verify_assign(options, ast); break;
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
    case TK_ERROR:        ast_seterror(ast); break;

    default:              ast_inheritflags(ast); break;
  }

  if(!r)
  {
    pony_assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}
