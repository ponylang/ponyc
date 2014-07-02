#include "typechecker.h"
#include "scope.h"
#include "valid.h"
#include "traits.h"
#include "sugar.h"
#include "expr.h"
#include "args.h"
#include <assert.h>

bool typecheck(ast_t* ast, int verbose)
{
  if(ast_visit(ast, pass_scope, NULL, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, pass_valid, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, pass_traits, NULL, AST_LEFT, verbose) != AST_OK)
    return false;

  // recalculate scopes in the presence of flattened traits
  ast_clear(ast);

  if(ast_visit(ast, pass_scope, NULL, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, pass_expr, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, pass_args, AST_LEFT, verbose) != AST_OK)
    return false;

  return true;
}
