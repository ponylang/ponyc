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

  ast_t* expr_type = ast_type(expr);
  ast_t* a_type = alias(expr_type);
  ast_t* type = NULL;

  // TODO: check for unreachable cases, check for exhaustive match if there is
  // no else branch
  ast_t* the_case = ast_child(cases);

  while(the_case != NULL)
  {
    ast_t* body = ast_childidx(the_case, 2);
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

static bool is_valid_pattern(ast_t* match_type, ast_t* pattern);

static bool is_valid_tuple_pattern(ast_t* match_type, ast_t* pattern)
{
  switch(ast_id(match_type))
  {
    case TK_UNIONTYPE:
    {
      // If some possible type can match, we can match.
      ast_t* match_child = ast_child(match_type);

      while(match_child != NULL)
      {
        if(is_valid_tuple_pattern(match_child, pattern))
          return true;

        match_child = ast_sibling(match_child);
      }

      return false;
    }

    case TK_ISECTTYPE:
    {
      // If every possible type can match, we can match.
      ast_t* match_child = ast_child(match_type);

      while(match_child != NULL)
      {
        if(!is_valid_tuple_pattern(match_child, pattern))
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
        if(!is_valid_pattern(match_child, pattern_child))
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

static bool is_valid_pattern(ast_t* match_type, ast_t* pattern)
{
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
        return false;
      }

      bool ok = could_subtype(a_type, capture_type);
      ast_free_unattached(a_type);

      if(!ok)
        ast_error(pattern, "this capture can never match");

      return ok;
    }

    case TK_TUPLE:
    {
      // Treat a one element tuple as a normal expression.
      ast_t* child = ast_child(pattern);

      if(ast_sibling(child) == NULL)
        return is_valid_pattern(match_type, child);

      return is_valid_tuple_pattern(match_type, pattern);
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

      return is_valid_pattern(match_type, child);
    }

    default:
    {
      // Structural equality, pattern.eq(match).
      ast_t* fun = lookup(pattern, ast_type(pattern), stringtab("eq"));

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

      if(ast_sibling(param) != NULL)
      {
        ast_error(pattern,
          "eq must take a single argument when pattern matching");
        ok = false;
      } else {
        ast_t* param_type;

        if(ast_id(param) == TK_PARAM)
          param_type = ast_childidx(param, 1);
        else
          param_type = param;

        ast_t* a_type = alias(match_type);
        bool sub_ok = could_subtype(a_type, param_type);
        ast_free_unattached(a_type);

        if(!sub_ok)
        {
          ast_error(pattern,
            "the match expression will never be a type that could be "
            "passed to eq on this pattern");
          ok = false;
        }
      }

      if(!ok)
        ast_error(fun, "definition of eq is here");

      return ok;
    }
  }

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

  ast_t* cases = ast_parent(ast);
  ast_t* match = ast_parent(cases);
  ast_t* match_expr = ast_child(match);
  ast_t* match_type = ast_type(match_expr);

  // TODO: Need to get the capability union for the match type so that pattern
  // matching can never recover capabilities.
  if(!is_valid_pattern(match_type, pattern))
  {
    ast_error(pattern, "this pattern can never match");
    return false;
  }

  if((ast_id(guard) != TK_NONE) && !is_bool(ast_type(guard)))
  {
    ast_error(guard, "guard must be a boolean expression");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}
