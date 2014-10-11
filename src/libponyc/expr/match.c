#include "match.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include <assert.h>

bool expr_match(ast_t* ast)
{
  assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  ast_t* expr_type = ast_type(expr);
  ast_t* a_type = alias(expr_type);
  ast_t* type = NULL;

  // TODO: check for unreachable cases, check for exhaustive match if there is
  // no else branch
  ast_t* the_case = ast_child(cases);

  while(the_case != NULL)
  {
    AST_GET_CHILDREN(the_case, pattern, guard, body);

    ast_t* pattern_type = ast_type(pattern);

    // TODO: This is too strict. Other than captures, we should allow patterns
    // where the pattern type could be a subtype of the expression type (without
    // aliasing) as well, since it is equivalent to identity.
    if(!could_subtype(a_type, pattern_type))
    {
      ast_error(pattern, "match expression can never be of this type");
      ast_free_unattached(a_type);
      return false;
    }

    ast_t* body_type = ast_type(body);
    type = type_union(type, body_type);

    the_case = ast_sibling(the_case);
  }

  ast_free_unattached(a_type);

  if(ast_id(else_clause) != TK_NONE)
  {
    ast_t* else_type = ast_type(else_clause);
    type = type_union(type, else_type);
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

static bool is_valid_pattern(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_TUPLE:
    {
      // A tuple is valid if every child is valid.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_valid_pattern(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_SEQ:
    {
      // TODO: allow this as well?
      // Only valid for single element sequences.
      ast_t* child = ast_child(ast);

      if(ast_sibling(child) != NULL)
      {
        ast_error(child, "patterns cannot contain sequences");
        return false;
      }

      return is_valid_pattern(child);
    }

    default: {}
  }

  // All other constructs are valid.
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

  if(!is_valid_pattern(pattern))
    return false;

  if((ast_id(guard) != TK_NONE) && !is_bool(ast_type(guard)))
  {
    ast_error(guard, "guard must be a boolean expression");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}
