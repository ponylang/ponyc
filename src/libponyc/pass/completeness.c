#include "completeness.h"
#include "ponyassert.h"

static bool completeness_match(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  size_t branch_count = 0;

  if(!ast_checkflag(cases, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, cases);
  }

  // An else_clause of TK_NONE doesn't cont as a branch because, we are after
  // exhaustive match checking is done so, an else clause of TK_NONE means that
  // we don't need the else and have an exhaustive match.
  if(ast_id(else_clause) != TK_NONE &&
    !ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    branch_count++;
    ast_inheritbranch(ast, else_clause);
  }

  // If all branches jump away with no value, then we do too.
  if(branch_count == 0)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  ast_consolidate_branches(ast, branch_count);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

ast_result_t pass_completeness(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_MATCH:
      completeness_match(options, ast); break;
    default:
      {}
  }

  return AST_OK;
}
