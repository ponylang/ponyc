#include "match.h"
#include "literal.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/lookup.h"
#include "../ast/stringtab.h"
#include "../ast/id.h"
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
    ast_error(opt->check.errors, expr,
      "cannot infer type for literal match expression");
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
    type = control_type_add_branch(opt, type, cases);
    ast_inheritbranch(ast, cases);
    branch_count++;
  }

  if(!is_control_type(else_type))
  {
    type = control_type_add_branch(opt, type, else_clause);
    ast_inheritbranch(ast, else_clause);
    branch_count++;
  }

  if(type == NULL)
  {
    if(ast_sibling(ast) != NULL)
    {
      ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
      return false;
    }

    type = ast_from(ast, TK_MATCH);
  }

  ast_settype(ast, type);
  ast_consolidate_branches(ast, branch_count);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_cases(pass_opt_t* opt, ast_t* ast)
{
  assert(ast_id(ast) == TK_CASES);
  ast_t* the_case = ast_child(ast);

  if(the_case == NULL)
  {
    ast_error(opt->check.errors, ast, "match must have at least one case");
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
      type = control_type_add_branch(opt, type, body);
      ast_inheritbranch(ast, the_case);
      branch_count++;
    }

    the_case = ast_sibling(the_case);
  }

  if(type == NULL)
    type = ast_from(ast, TK_CASES);

  ast_settype(ast, type);
  ast_consolidate_branches(ast, branch_count);
  return true;
}

static ast_t* make_pattern_type(pass_opt_t* opt, ast_t* pattern)
{
  if(ast_id(pattern) == TK_NONE)
  {
    ast_t* type = ast_from(pattern, TK_DONTCARETYPE);
    ast_settype(pattern, type);
    return type;
  }

  ast_t* pattern_type = ast_type(pattern);

  if(is_control_type(pattern_type))
  {
    ast_error(opt->check.errors, pattern, "not a matchable pattern");
    return NULL;
  }

  switch(ast_id(pattern))
  {
    case TK_DONTCAREREF:
    case TK_MATCH_CAPTURE:
    case TK_MATCH_DONTCARE:
      return ast_type(pattern);

    case TK_TUPLE:
    {
      ast_t* pattern_child = ast_child(pattern);

      // Treat a one element tuple as a normal expression.
      if(ast_sibling(pattern_child) == NULL)
        return make_pattern_type(opt, pattern_child);

      // Check every element pairwise.
      ast_t* tuple_type = ast_from(pattern, TK_TUPLETYPE);
      bool ok = true;

      while(pattern_child != NULL)
      {
        ast_t* child_type = make_pattern_type(opt, pattern_child);

        if(child_type != NULL)
          ast_append(tuple_type, child_type);
        else
          ok = false;

        pattern_child = ast_sibling(pattern_child);
      }

      if(!ok)
      {
        ast_free_unattached(tuple_type);
        tuple_type = NULL;
      }

      return tuple_type;
    }

    case TK_SEQ:
    {
      // This may be just a capture.
      if(ast_childcount(pattern) == 1)
        return make_pattern_type(opt, ast_child(pattern));

      // Treat this like other nodes.
      break;
    }

    default:
      break;
  }

  // Structural equality, pattern.eq(match).
  ast_t* fun = lookup(opt, pattern, pattern_type, stringtab("eq"));

  if(fun == NULL)
  {
    ast_error(opt->check.errors, pattern,
      "this pattern element doesn't support structural equality");

    return NULL;
  }

  if(ast_id(fun) != TK_FUN)
  {
    ast_error(opt->check.errors, pattern,
      "eq is not a function on this pattern element");
    ast_error_continue(opt->check.errors, fun,
      "definition of eq is here");
    ast_free_unattached(fun);
    return NULL;
  }

  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, partial);
  bool ok = true;

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, pattern,
      "polymorphic eq not supported in pattern matching");
    ok = false;
  }

  if(!is_bool(result))
  {
    ast_error(opt->check.errors, pattern,
      "eq must return Bool when pattern matching");
    ok = false;
  }

  if(ast_id(partial) != TK_NONE)
  {
    ast_error(opt->check.errors, pattern,
      "eq cannot be partial when pattern matching");
    ok = false;
  }

  ast_t* r_type = set_cap_and_ephemeral(pattern_type, ast_id(cap), TK_NONE);
  ast_t* a_type = alias(pattern_type);
  errorframe_t info = NULL;

  if(!is_subtype(a_type, r_type, &info, opt))
  {
    errorframe_t frame = NULL;
    ast_error_frame(&frame, pattern, "eq cannot be called on this pattern");
    errorframe_append(&frame, &info);
    errorframe_report(&frame, opt->check.errors);
    ok = false;
  }

  ast_t* param = ast_child(params);

  if(param == NULL || ast_sibling(param) != NULL)
  {
    ast_error(opt->check.errors, pattern,
      "eq must take a single argument when pattern matching");

    ok = false;
  }

  if(ok)
  {
    AST_GET_CHILDREN(param, param_id, param_type);
    pattern_type = ast_dup(param_type);
  } else {
    pattern_type = NULL;
  }

  ast_free_unattached(r_type);
  ast_free_unattached(a_type);
  ast_free_unattached(fun);

  return pattern_type;
}

// Infer the types of any literals in the pattern of the given case
static bool infer_pattern_type(ast_t* pattern, ast_t* match_expr_type,
  pass_opt_t* opt)
{
  assert(pattern != NULL);
  assert(match_expr_type != NULL);

  if(is_type_literal(match_expr_type))
  {
    ast_error(opt->check.errors, match_expr_type,
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
    ast_error(opt->check.errors, ast,
      "can't have a case with no conditions, use an else clause");
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

  ast_t* pattern_type = make_pattern_type(opt, pattern);

  if(pattern_type == NULL)
    return false;

  ast_settype(ast, pattern_type);

  ast_t* operand_type = alias(match_type);
  bool ok = true;

  switch(is_matchtype(operand_type, pattern_type, opt))
  {
    case MATCHTYPE_ACCEPT:
      break;

    case MATCHTYPE_REJECT:
    {
      ast_error(opt->check.errors, pattern, "this pattern can never match");
      ast_error_continue(opt->check.errors, match_type, "match type: %s",
        ast_print_type(operand_type));
      ast_error_continue(opt->check.errors, pattern, "pattern type: %s",
        ast_print_type(pattern_type));
      ok = false;
      break;
    }

    case MATCHTYPE_DENY:
    {
      ast_error(opt->check.errors, pattern,
        "this capture violates capabilities");
      ast_error_continue(opt->check.errors, match_type, "match type: %s",
        ast_print_type(operand_type));
      ast_error_continue(opt->check.errors, pattern, "pattern type: %s",
        ast_print_type(pattern_type));
      ok = false;
      break;
    }
  }

  if(ast_id(guard) != TK_NONE)
  {
    ast_t* guard_type = ast_type(guard);

    if(is_typecheck_error(guard_type))
    {
      ok = false;
    } else if(!is_bool(guard_type)) {
      ast_error(opt->check.errors, guard,
        "guard must be a boolean expression");
      ok = false;
    }
  }

  ast_free_unattached(operand_type);
  return ok;
}

bool expr_match_capture(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert(ast != NULL);

  AST_GET_CHILDREN(ast, id, type);

  if(is_name_dontcare(ast_name(id)))
    ast_setid(ast, TK_MATCH_DONTCARE);

  assert(type != NULL);

  // Capture type is as specified.
  ast_settype(ast, type);
  ast_settype(ast_child(ast), type);
  return true;
}
