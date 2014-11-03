#include "match.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/lookup.h"
#include "../ds/stringtab.h"
#include <assert.h>

bool expr_match(ast_t* ast)
{
  assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  ast_t* cases_type = ast_type(cases);
  ast_t* else_type = ast_type(else_clause);

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(cases_type != NULL)
  {
    type = type_union(type, cases_type);
    ast_inheritbranch(ast, cases);
    branch_count++;
  }

  if(else_type != NULL)
  {
    type = type_union(type, else_type);
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
      type = type_union(type, body_type);
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

static bool is_valid_pattern(ast_t* match_type, ast_t* pattern, bool errors);

static bool is_valid_tuple_pattern(ast_t* match_type, ast_t* pattern,
  bool errors)
{
  switch(ast_id(match_type))
  {
    case TK_UNIONTYPE:
    {
      // If some possible type can match, we can match.
      ast_t* match_child = ast_child(match_type);

      while(match_child != NULL)
      {
        if(is_valid_tuple_pattern(match_child, pattern, false))
          return true;

        match_child = ast_sibling(match_child);
      }

      ast_error(pattern,
        "no possible type of the match expression can match this pattern");

      ast_error(match_type, "match type: %s",
        ast_print_type(alias(match_type)));

      ast_error(pattern, "pattern type: %s",
        ast_print_type(ast_type(pattern)));

      return false;
    }

    case TK_ISECTTYPE:
    {
      // If every possible type can match, we can match.
      ast_t* match_child = ast_child(match_type);

      while(match_child != NULL)
      {
        if(!is_valid_tuple_pattern(match_child, pattern, errors))
          return false;

        match_child = ast_sibling(match_child);
      }

      return true;
    }

    case TK_TUPLETYPE:
    {
      // Check for a cardinality match.
      if(ast_childcount(match_type) != ast_childcount(pattern))
        return false;

      // Check every element pairwise.
      ast_t* match_child = ast_child(match_type);
      ast_t* pattern_child = ast_child(pattern);

      while(match_child != NULL)
      {
        if(!is_valid_pattern(match_child, pattern_child, errors))
          return false;

        match_child = ast_sibling(match_child);
        pattern_child = ast_sibling(pattern_child);
      }

      return true;
    }

    default: {}
  }

  // The match type is not a tuple nor does it contain a tuple, so it cannot
  // match a tuple.
  return false;
}

static bool is_valid_pattern(ast_t* match_type, ast_t* pattern, bool errors)
{
  if(ast_type(pattern) == NULL)
  {
    if(errors)
      ast_error(pattern, "not a matchable pattern");

    return false;
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
        if(errors)
          ast_error(capture_type,
            "can't capture a tuple, change this into a tuple of capture "
            "expressions");

        return false;
      }

      bool ok = could_subtype(a_type, capture_type);

      if(!ok && errors)
      {
        ast_error(pattern, "this capture can never match");
        ast_error(a_type, "match type alias: %s", ast_print_type(a_type));
        ast_error(capture_type, "capture type: %s",
          ast_print_type(capture_type));
      }

      ast_free_unattached(a_type);
      return ok;
    }

    case TK_TUPLE:
    {
      // Treat a one element tuple as a normal expression.
      ast_t* child = ast_child(pattern);

      if(ast_sibling(child) == NULL)
        return is_valid_pattern(match_type, child, errors);

      return is_valid_tuple_pattern(match_type, pattern, errors);
    }

    case TK_SEQ:
    {
      // Patterns cannot contain sequences.
      ast_t* child = ast_child(pattern);
      ast_t* next = ast_sibling(child);

      if(next != NULL)
      {
        if(errors)
          ast_error(next, "expression in patterns cannot be sequences");

        return false;
      }

      return is_valid_pattern(match_type, child, errors);
    }

    default:
    {
      // Structural equality, pattern.eq(match).
      ast_t* fun = lookup(pattern, ast_type(pattern), stringtab("eq"));

      if(fun == NULL)
      {
        if(errors)
          ast_error(pattern,
            "this pattern element doesn't support structural equality");

        return false;
      }

      if(ast_id(fun) != TK_FUN)
      {
        if(errors)
        {
          ast_error(pattern, "eq is not a function on this pattern element");
          ast_error(fun, "definition of eq is here");
        }

        return false;
      }

      AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, partial);
      bool ok = true;

      if(ast_id(typeparams) != TK_NONE)
      {
        if(errors)
          ast_error(pattern,
            "polymorphic eq not supported in pattern matching");

        ok = false;
      }

      if(!is_bool(result))
      {
        if(errors)
          ast_error(pattern, "eq must return Bool when pattern matching");

        ok = false;
      }

      if(ast_id(partial) != TK_NONE)
      {
        if(errors)
          ast_error(pattern, "eq cannot be partial when pattern matching");

        ok = false;
      }

      ast_t* param = ast_child(params);

      if(ast_sibling(param) != NULL)
      {
        if(errors)
          ast_error(pattern,
            "eq must take a single argument when pattern matching");

        ok = false;
      } else {
        ast_t* param_type = ast_childidx(param, 1);
        ast_t* a_type = alias(match_type);
        bool sub_ok = could_subtype(a_type, param_type);
        ast_free_unattached(a_type);

        if(!sub_ok)
        {
          if(errors)
            ast_error(pattern,
              "the match expression will never be a type that could be "
              "passed to eq on this pattern");

          ok = false;
        }
      }

      if(!ok && errors)
        ast_error(fun, "definition of eq is here");

      return ok;
    }
  }

  return true;
}

bool expr_case(ast_t* ast)
{
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

  // TODO: Need to get the capability union for the match type so that pattern
  // matching can never recover capabilities.
  if(!is_valid_pattern(match_type, pattern, true))
    return false;

  if((ast_id(guard) != TK_NONE) && !is_bool(ast_type(guard)))
  {
    ast_error(guard, "guard must be a boolean expression");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}
