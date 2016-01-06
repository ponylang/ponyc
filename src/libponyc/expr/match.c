#include "match.h"
#include "literal.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/lookup.h"
#include "../ast/stringtab.h"
#include "../pass/expr.h"
#include "../pass/pass.h"
#include <assert.h>

bool expr_match(pass_opt_t* opt, ast_t* ast)
{
  assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  // A literal match expression should have been caught by the cases, but check
  // again to avoid an assert if we've missed a case
  ast_t* expr_type = ast_type(expr);

  if(is_typecheck_error(expr_type))
    return false;

  if(is_type_literal(expr_type))
  {
    ast_error(expr, "cannot infer type for literal match expression");
    return false;
  }

  ast_t* cases_type = ast_type(cases);
  ast_t* else_type = ast_type(else_clause);

  if(is_typecheck_error(cases_type) || is_typecheck_error(else_type))
    return false;

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(!is_control_type(cases_type))
  {
    type = control_type_add_branch(type, cases);
    ast_inheritbranch(ast, cases);
    branch_count++;
  }

  if(!is_control_type(else_type))
  {
    type = control_type_add_branch(type, else_clause);
    ast_inheritbranch(ast, else_clause);
    branch_count++;
  }

  if(type == NULL)
  {
    if(ast_sibling(ast) != NULL)
    {
      ast_error(ast_sibling(ast), "unreachable code");
      return false;
    }

    type = ast_from(ast, TK_MATCH);
  }

  ast_settype(ast, type);
  ast_inheritflags(ast);
  ast_consolidate_branches(ast, branch_count);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_cases(ast_t* ast)
{
  assert(ast_id(ast) == TK_CASES);
  ast_t* the_case = ast_child(ast);

  if(the_case == NULL)
  {
    ast_error(ast, "match must have at least one case");
    return false;
  }

  ast_t* type = NULL;
  size_t branch_count = 0;

  while(the_case != NULL)
  {
    AST_GET_CHILDREN(the_case, pattern, guard, body);
    ast_t* body_type = ast_type(body);

    if(!is_typecheck_error(body_type) && !is_control_type(body_type))
    {
      type = control_type_add_branch(type, body);
      ast_inheritbranch(ast, the_case);
      branch_count++;
    }

    the_case = ast_sibling(the_case);
  }

  if(type == NULL)
    type = ast_from(ast, TK_CASES);

  ast_settype(ast, type);
  ast_inheritflags(ast);
  ast_consolidate_branches(ast, branch_count);
  return true;
}

static bool is_valid_pattern(pass_opt_t* opt, ast_t* pattern)
{
  if(ast_id(pattern) == TK_NONE)
  {
    ast_settype(pattern, ast_from(pattern, TK_DONTCARE));
    return true;
  }

  ast_t* pattern_type = ast_type(pattern);

  if(is_control_type(pattern_type))
  {
    ast_error(pattern, "not a matchable pattern");
    return false;
  }

  switch(ast_id(pattern))
  {
    case TK_VAR:
    case TK_LET:
    {
      // Disallow capturing tuples.
      AST_GET_CHILDREN(pattern, id, capture_type);

      if(ast_id(capture_type) == TK_TUPLETYPE)
      {
        ast_error(capture_type,
          "can't capture a tuple, change this into a tuple of capture "
          "expressions");

        return false;
      }

      // Set the pattern type to be the capture type.
      ast_settype(pattern, capture_type);
      return true;
    }

    case TK_TUPLE:
    {
      ast_t* pattern_child = ast_child(pattern);

      // Treat a one element tuple as a normal expression.
      if(ast_sibling(pattern_child) == NULL)
      {
        bool ok = is_valid_pattern(opt, pattern_child);
        ast_settype(pattern, ast_type(pattern_child));
        return ok;
      }

      // Check every element pairwise.
      ast_t* pattern_type = ast_from(pattern, TK_TUPLETYPE);
      bool ok = true;

      while(pattern_child != NULL)
      {
        if(!is_valid_pattern(opt, pattern_child))
          ok = false;

        ast_append(pattern_type, ast_type(pattern_child));
        pattern_child = ast_sibling(pattern_child);
      }

      ast_settype(pattern, pattern_type);
      return ok;
    }

    case TK_SEQ:
    {
      // Patterns cannot contain sequences.
      ast_t* child = ast_child(pattern);
      ast_t* next = ast_sibling(child);

      if(next != NULL)
      {
        ast_error(next, "expression in patterns cannot be sequences");
        return false;
      }

      bool ok = is_valid_pattern(opt, child);
      ast_settype(pattern, ast_type(child));
      return ok;
    }

    case TK_DONTCARE:
      // It's always ok not to care.
      return true;

    default:
    {
      // Structural equality, pattern.eq(match).
      ast_t* fun = lookup(opt, pattern, pattern_type, stringtab("eq"));

      if(fun == NULL)
      {
        ast_error(pattern,
          "this pattern element doesn't support structural equality");

        return false;
      }

      if(ast_id(fun) != TK_FUN)
      {
        ast_error(pattern, "eq is not a function on this pattern element");
        ast_error(fun, "definition of eq is here");
        ast_free_unattached(fun);
        return false;
      }

      AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, partial);
      bool ok = true;

      if(ast_id(typeparams) != TK_NONE)
      {
        ast_error(pattern, "polymorphic eq not supported in pattern matching");
        ok = false;
      }

      if(!is_bool(result))
      {
        ast_error(pattern, "eq must return Bool when pattern matching");
        ok = false;
      }

      if(ast_id(partial) != TK_NONE)
      {
        ast_error(pattern, "eq cannot be partial when pattern matching");
        ok = false;
      }

      ast_t* param = ast_child(params);

      if(param == NULL || ast_sibling(param) != NULL)
      {
        ast_error(pattern,
          "eq must take a single argument when pattern matching");

        ok = false;
      } else {
        AST_GET_CHILDREN(param, param_id, param_type);
        ast_settype(pattern, param_type);
      }

      ast_free_unattached(fun);
      return ok;
    }
  }

  assert(0);
  return false;
}

// Infer the types of any literals in the pattern of the given case
static bool infer_pattern_type(ast_t* pattern, ast_t* match_expr_type,
  pass_opt_t* opt)
{
  assert(pattern != NULL);
  assert(match_expr_type != NULL);

  if(is_type_literal(match_expr_type))
  {
    ast_error(match_expr_type,
      "cannot infer type for literal match expression");
    return false;
  }

  return coerce_literals(&pattern, match_expr_type, opt);
}

bool expr_case(pass_opt_t* opt, ast_t* ast)
{
  assert(opt != NULL);
  assert(ast_id(ast) == TK_CASE);
  AST_GET_CHILDREN(ast, pattern, guard, body);

  if((ast_id(pattern) == TK_NONE) && (ast_id(guard) == TK_NONE))
  {
    ast_error(ast, "can't have a case with no conditions, use an else clause");
    return false;
  }

  ast_t* cases = ast_parent(ast);
  ast_t* match = ast_parent(cases);
  ast_t* match_expr = ast_child(match);
  ast_t* match_type = ast_type(match_expr);

  if(is_control_type(match_type) || is_typecheck_error(match_type))
    return false;

  if(!infer_pattern_type(pattern, match_type, opt))
    return false;

  if(!is_valid_pattern(opt, pattern))
    return false;

  ast_t* operand_type = alias(match_type);
  ast_t* pattern_type = ast_type(pattern);
  bool ok = true;

  switch(is_matchtype(operand_type, pattern_type))
  {
    case MATCHTYPE_ACCEPT:
      break;

    case MATCHTYPE_REJECT:
      ast_error(pattern, "this pattern can never match");
      ast_error(match_type, "match type: %s", ast_print_type(operand_type));
      ast_error(pattern, "pattern type: %s", ast_print_type(pattern_type));
      ok = false;
      break;

    case MATCHTYPE_DENY:
      ast_error(pattern, "this capture violates capabilities");
      ast_error(match_type, "match type: %s", ast_print_type(operand_type));
      ast_error(pattern, "pattern type: %s", ast_print_type(pattern_type));
      ok = false;
      break;
  }

  if(ast_id(guard) != TK_NONE)
  {
    ast_t* guard_type = ast_type(guard);

    if(is_typecheck_error(guard_type))
    {
      ok = false;
    }
    else if(!is_bool(guard_type))
    {
      ast_error(guard, "guard must be a boolean expression");
      ok = false;
    }
  }

  ast_free_unattached(operand_type);
  ast_inheritflags(ast);
  return ok;
}
