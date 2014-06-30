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
  if(ast_visit(ast, type_scope, NULL, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, type_valid, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, type_traits, NULL, AST_LEFT, verbose) != AST_OK)
    return false;

  // recalculate scopes in the presence of flattened traits
  ast_clear(ast);

  if(ast_visit(ast, type_scope, NULL, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, type_expr, AST_LEFT, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, type_args, AST_LEFT, verbose) != AST_OK)
    return false;

  return true;
}
