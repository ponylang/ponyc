#include "control.h"
#include "ponyassert.h"

bool verify_try(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_TRY) || (ast_id(ast) == TK_TRY_NO_CHECK));
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  // It has to be possible for the left side to result in an error.
  if((ast_id(ast) != TK_TRY_NO_CHECK) && !ast_canerror(body))
  {
    ast_error(opt->check.errors, body,
      "try expression never results in an error");
    return false;
  }

  // Doesn't inherit error from the body.
  if(ast_canerror(else_clause) || ast_canerror(then_clause))
    ast_seterror(ast);

  if(ast_cansend(body) || ast_cansend(else_clause) || ast_cansend(then_clause))
    ast_setsend(ast);

  if(ast_mightsend(body) || ast_mightsend(else_clause) ||
    ast_mightsend(then_clause))
    ast_setmightsend(ast);

  return true;
}

bool verify_disposing_block(ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DISPOSING_BLOCK);
  AST_GET_CHILDREN(ast, body, dispose_clause);

  if(ast_canerror(body))
    ast_seterror(ast);

  if(ast_cansend(body) || ast_cansend(dispose_clause))
    ast_setsend(ast);

  if(ast_mightsend(body) || ast_mightsend(dispose_clause))
    ast_setmightsend(ast);

  return true;
}
