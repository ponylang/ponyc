#include "match.h"
#include "literal.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/lookup.h"
#include "../ast/stringtab.h"
#include "../pass/pass.h"
#include <assert.h>

bool expr_match(ast_t* ast)
{
  assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  // A literal match expression should have been caught by the cases, but check
  // again to avoid an assert if we've missed a case
  if(is_type_literal(ast_type(expr)))
  {
    ast_error(expr, "Cannot infer type for literal match expression");
    return false;
  }

  ast_t* cases_type = ast_type(cases);
  ast_t* else_type = ast_type(else_clause);

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(cases_type != NULL)
  {
    type = control_type_add_branch(type, cases);
    ast_inheritbranch(ast, cases);
    branch_count++;
  }

  if(else_type != NULL)
  {
    type = control_type_add_branch(type, else_clause);
    ast_inheritbranch(ast, else_clause);
    branch_count++;
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  ast_consolidate_branches(ast, branch_count);

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

    if(body_type != NULL)
    {
      type = control_type_add_branch(type, body);
      ast_inheritbranch(ast, the_case);
      branch_count++;
    }

    the_case = ast_sibling(the_case);
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  ast_consolidate_branches(ast, branch_count);
  return true;
}

static matchtype_t is_valid_pattern(typecheck_t* t, ast_t* match_type,
  ast_t* pattern, bool errors);

static matchtype_t is_valid_tuple_pattern(typecheck_t* t, ast_t* match_type,
  ast_t* pattern, bool errors)
{
  switch(ast_id(match_type))
  {
    case TK_UNIONTYPE:
    {
      // If some possible type can match, we can match.
      ast_t* match_child = ast_child(match_type);
      matchtype_t ok = MATCHTYPE_REJECT;

      while(match_child != NULL)
      {
        matchtype_t sub_ok = is_valid_tuple_pattern(t, match_child, pattern,
          false);

        if(sub_ok != MATCHTYPE_REJECT)
          ok = sub_ok;

        if(ok == MATCHTYPE_DENY)
          break;

        match_child = ast_sibling(match_child);
      }

      if((ok != MATCHTYPE_ACCEPT) && errors)
      {
        ast_error(pattern,
          "no possible type of the match expression can match this pattern");

        ast_error(match_type, "match type: %s",
          ast_print_type(alias(match_type)));

        ast_error(pattern, "pattern type: %s",
          ast_print_type(ast_type(pattern)));
      }

      return ok;
    }

    case TK_ISECTTYPE:
    {
      // If every possible type can match, we can match.
      ast_t* match_child = ast_child(match_type);

      while(match_child != NULL)
      {
        matchtype_t ok = is_valid_tuple_pattern(t, match_child, pattern,
          errors);

        if(ok != MATCHTYPE_ACCEPT)
          return ok;

        match_child = ast_sibling(match_child);
      }

      return MATCHTYPE_ACCEPT;
    }

    case TK_TUPLETYPE:
    {
      // Check for a cardinality match.
      if(ast_childcount(match_type) != ast_childcount(pattern))
      {
        if(errors)
        {
          ast_error(pattern,
            "pattern and type are tuples of different cardinality");
        }

        return MATCHTYPE_REJECT;
      }

      // Check every element pairwise.
      ast_t* match_child = ast_child(match_type);
      ast_t* pattern_child = ast_child(pattern);

      while(match_child != NULL)
      {
        matchtype_t ok = is_valid_pattern(t, match_child, pattern_child,
          errors);

        if(ok != MATCHTYPE_ACCEPT)
        {
          if(errors)
          {
            ast_error(pattern, "type can never match tuple element");
            ast_error(match_child, "match type: %s",
              ast_print_type(match_child));
            ast_error(pattern_child, "pattern type: %s",
              ast_print_type(ast_type(pattern_child)));
          }
          return ok;
        }

        match_child = ast_sibling(match_child);
        pattern_child = ast_sibling(pattern_child);
      }

      return MATCHTYPE_ACCEPT;
    }

    default: {}
  }

  // The match type is not a tuple nor does it contain a tuple, so it cannot
  // match a tuple.
  return MATCHTYPE_REJECT;
}

static matchtype_t is_valid_pattern(typecheck_t* t, ast_t* match_type,
  ast_t* pattern, bool errors)
{
  if(ast_type(pattern) == NULL)
  {
    ast_error(pattern, "not a matchable pattern");
    return MATCHTYPE_DENY;
  }

  switch(ast_id(pattern))
  {
    case TK_VAR:
    case TK_LET:
    {
      // There must exist some subtype of an alias of match_type that is a
      // subtype of the capture type.
      ast_t* a_type = alias(match_type);
      ast_t* capture_type = ast_childidx(pattern, 1);

      // Disallow capturing tuples.
      if(ast_id(capture_type) == TK_TUPLETYPE)
      {
        ast_error(capture_type,
          "can't capture a tuple, change this into a tuple of capture "
          "expressions");

        return MATCHTYPE_DENY;
      }

      matchtype_t ok = could_subtype(a_type, capture_type);

      if((ok != MATCHTYPE_ACCEPT) && errors)
      {
        ast_error(pattern, "this capture can never match");
        ast_error(a_type, "match type alias: %s", ast_print_type(a_type));
        ast_error(capture_type, "capture type: %s",
          ast_print_type(capture_type));
      }

      if(contains_interface(capture_type))
      {
        ast_error(pattern, "this capture type contains an interface, which "
          "cannot be matched dynamically"
          );

        ok = MATCHTYPE_DENY;
      }

      ast_free_unattached(a_type);
      return ok;
    }

    case TK_TUPLE:
    {
      // Treat a one element tuple as a normal expression.
      ast_t* child = ast_child(pattern);

      if(ast_sibling(child) == NULL)
        return is_valid_pattern(t, match_type, child, errors);

      return is_valid_tuple_pattern(t, match_type, pattern, errors);
    }

    case TK_SEQ:
    {
      // Patterns cannot contain sequences.
      ast_t* child = ast_child(pattern);
      ast_t* next = ast_sibling(child);

      if(next != NULL)
      {
        ast_error(next, "expression in patterns cannot be sequences");
        return MATCHTYPE_DENY;
      }

      return is_valid_pattern(t, match_type, child, errors);
    }

    case TK_DONTCARE:
      // It's always ok not to care.
      return MATCHTYPE_ACCEPT;

    default:
    {
      // Structural equality, pattern.eq(match).
      ast_t* fun = lookup(t, pattern, ast_type(pattern), stringtab("eq"));

      if(fun == NULL)
      {
        ast_error(pattern,
          "this pattern element doesn't support structural equality");

        return MATCHTYPE_DENY;
      }

      if(ast_id(fun) != TK_FUN)
      {
        ast_error(pattern, "eq is not a function on this pattern element");
        ast_error(fun, "definition of eq is here");
        return MATCHTYPE_DENY;
      }

      AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, partial);
      matchtype_t ok = MATCHTYPE_ACCEPT;

      if(ast_id(typeparams) != TK_NONE)
      {
        ast_error(pattern,
          "polymorphic eq not supported in pattern matching");

        ok = MATCHTYPE_DENY;
      }

      if(!is_bool(result))
      {
        ast_error(pattern, "eq must return Bool when pattern matching");
        ok = MATCHTYPE_DENY;
      }

      if(ast_id(partial) != TK_NONE)
      {
        ast_error(pattern, "eq cannot be partial when pattern matching");
        ok = MATCHTYPE_DENY;
      }

      ast_t* param = ast_child(params);

      if(ast_sibling(param) != NULL)
      {
        ast_error(pattern,
          "eq must take a single argument when pattern matching");

        ok = MATCHTYPE_DENY;
      } else {
        ast_t* param_type = ast_childidx(param, 1);
        ast_t* a_type = alias(match_type);
        ok = could_subtype(a_type, param_type);
        ast_free_unattached(a_type);

        if((ok != MATCHTYPE_ACCEPT) && errors)
        {
          ast_error(pattern,
            "the match expression will never be a type that could be "
            "passed to eq on this pattern");
        }

        if(contains_interface(param_type))
        {
          ast_error(pattern, "the parameter of eq on this pattern contains "
            "an interface, which cannot be matched dynamically"
            );

          ok = MATCHTYPE_DENY;
        }
      }

      if((ok != MATCHTYPE_ACCEPT) && errors)
        ast_error(fun, "definition of eq is here");

      return ok;
    }
  }

  return MATCHTYPE_ACCEPT;
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
      "Cannot infer type for literal match expression");
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

  if(match_type == NULL)
  {
    ast_error(match_expr, "not a matchable expression");
    return false;
  }

  if(!infer_pattern_type(pattern, match_type, opt))
    return false;

  matchtype_t is_valid = is_valid_pattern(&opt->check, match_type, pattern, true);
  if(is_valid != MATCHTYPE_ACCEPT)
    return false;

  if((ast_id(guard) != TK_NONE) && !is_bool(ast_type(guard)))
  {
    ast_error(guard, "guard must be a boolean expression");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}
