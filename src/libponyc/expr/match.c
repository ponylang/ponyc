#include "match.h"
#include "../type/subtype.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include <assert.h>

bool expr_match(ast_t* ast)
{
  assert(ast_id(ast) == TK_MATCH);
  ast_t* expr = ast_child(ast);
  ast_t* cases = ast_sibling(expr);
  ast_t* right = ast_sibling(cases);

  ast_t* the_case = ast_child(cases);
  ast_t* type = NULL;

  // TODO: check for unreachable cases
  // TODO: check for exhaustive match if there is no else branch
  while(the_case != NULL)
  {
    ast_t* body = ast_childidx(the_case, 3);
    ast_t* body_type = ast_type(body);
    type = type_union(ast, type, body_type);

    the_case = ast_sibling(the_case);
  }

  if(ast_id(right) != TK_NONE)
  {
    ast_t* r_type = ast_type(right);
    type = type_union(ast, type, r_type);
  } else {
    // TODO: remove this when we know there is exhaustive match
    type = type_union(ast, type, nominal_builtin(ast, "None"));
  }

  ast_settype(ast, type);
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

  return true;
}

bool expr_case(ast_t* ast)
{
  assert(ast_id(ast) == TK_CASE);

  ast_t* pattern = ast_child(ast);
  ast_t* as = ast_sibling(pattern);
  ast_t* guard = ast_sibling(as);

  if((ast_id(pattern) == TK_NONE) &&
    (ast_id(as) == TK_NONE) &&
    (ast_id(guard) == TK_NONE))
  {
    ast_error(ast, "can't have a case with no conditions, use an else clause");
    return false;
  }

  // TODO: check pattern is a subtype of the match expression?
  // are patterns structural equality? does the match expression have to be
  // Comparable?

  if((ast_id(guard) != TK_NONE) && (type_bool(guard) == NULL))
  {
    ast_error(guard, "guard must be a Bool");
    return false;
  }

  return true;
}

bool expr_as(ast_t* ast)
{
  assert(ast_id(ast) == TK_AS);

  ast_t* the_case = ast_parent(ast);
  assert(ast_id(the_case) == TK_CASE);

  ast_t* cases = ast_parent(the_case);
  assert(ast_id(cases) == TK_CASES);

  ast_t* match = ast_parent(cases);
  assert(ast_id(match) == TK_MATCH);

  ast_t* expr = ast_child(match);
  ast_t* expr_type = ast_type(expr);
  ast_t* match_type = ast_childidx(ast, 1);

  if(!is_subtype(ast, match_type, expr_type))
  {
    ast_error(match_type,
      "matching type is not a subtype of the expression type");
    return false;
  }

  return true;
}
