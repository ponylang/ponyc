#include "match.h"
#include "../ast/token.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include <assert.h>

bool expr_match(ast_t* ast)
{
  assert(ast_id(ast) == TK_MATCH);
  ast_t* expr = ast_child(ast);
  ast_t* cases = ast_sibling(expr);
  ast_t* else_clause = ast_sibling(cases);

  ast_t* the_case = ast_child(cases);
  ast_t* type = NULL;

  // TODO: check for unreachable cases
  // TODO: check for exhaustive match if there is no else branch
  while(the_case != NULL)
  {
    ast_t* body = ast_childidx(the_case, 2);
    ast_t* body_type = ast_type(body);
    type = type_union(type, body_type);
    type = type_literal_to_runtime(type);
    the_case = ast_sibling(the_case);
  }

  if(ast_id(else_clause) != TK_NONE)
  {
    ast_t* else_type = ast_type(else_clause);
    type = type_union(type, else_type);
    type = type_literal_to_runtime(type);
  } else {
    // TODO: remove this when we know there is exhaustive match
    type = type_union(type, type_builtin(ast, "None"));
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_cases(ast_t* ast)
{
  assert(ast_id(ast) == TK_CASES);
  ast_t* child = ast_child(ast);

  if(child == NULL)
  {
    ast_error(ast, "match must have at least one case");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}

bool expr_case(ast_t* ast)
{
  assert(ast_id(ast) == TK_CASE);

  ast_t* pattern = ast_child(ast);
  ast_t* guard = ast_sibling(pattern);

  if((ast_id(pattern) == TK_NONE) && (ast_id(guard) == TK_NONE))
  {
    ast_error(ast, "can't have a case with no conditions, use an else clause");
    return false;
  }

  // TODO: check pattern is a subtype of the match expression?
  // are patterns structural equality? does the match expression have to be
  // Comparable?

  if((ast_id(guard) != TK_NONE) && !is_bool(ast_type(guard)))
  {
    ast_error(guard, "guard must be a Bool");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}
