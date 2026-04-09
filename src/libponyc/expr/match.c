#include "match.h"
#include "literal.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/lookup.h"
#include "../type/cap.h"
#include "../type/typealias.h"
#include "../ast/astbuild.h"
#include "../ast/stringtab.h"
#include "../ast/id.h"
#include "../expr/control.h"
#include "../expr/reference.h"
#include "../pass/expr.h"
#include "../pass/pass.h"
#include "ponyassert.h"


static bool case_expr_matches_type_alone(pass_opt_t* opt, ast_t* case_expr)
{
  switch(ast_id(case_expr))
  {
    // These pattern forms match on type alone.
    case TK_MATCH_CAPTURE:
    case TK_MATCH_DONTCARE:
      return true;

    // This pattern form matches any type (ignoring the value entirely).
    case TK_DONTCAREREF:
      return true;

    // Tuple patterns may contain a mixture of matching on types and values,
    // so we only return true if *all* elements return true.
    case TK_TUPLE:
      for(ast_t* e = ast_child(case_expr); e != NULL; e = ast_sibling(e))
        if(!case_expr_matches_type_alone(opt, e))
          return false;
      return true;

    // This pattern form matches on type alone if the final child returns true.
    case TK_SEQ:
      return case_expr_matches_type_alone(opt, ast_childlast(case_expr));

    default: {}
  }

  // For all other case expression forms, we're matching on structural equality,
  // which can only be supported for primitives that use the default eq method,
  // and are not "special" builtin primitives that can have more than one
  // distinct value (machine words).
  // A value expression pattern that meets these conditions may be treated as
  // matching on type alone, because the value is a singleton for that type
  // and the result of the eq method will always be true if the type matches.

  ast_t* type = ast_type(case_expr);

  if(is_typecheck_error(type))
    return false;

  ast_t* def = (ast_t*)ast_data(type);

  if((def == NULL) || (ast_id(def) != TK_PRIMITIVE) || is_machine_word(type))
    return false;

  ast_t* eq_def = ast_get(def, stringtab("eq"), NULL);
  pony_assert(ast_id(eq_def) == TK_FUN);

  ast_t* eq_params = ast_childidx(eq_def, 3);
  pony_assert(ast_id(eq_params) == TK_PARAMS);
  pony_assert(ast_childcount(eq_params) == 1);

  ast_t* eq_body = ast_childidx(eq_def, 6);
  pony_assert(ast_id(eq_body) == TK_SEQ);

  // Expect to see a body containing the following AST:
  //   (is (this) (paramref (id that)))
  // where `that` is the name of the first parameter.
  if((ast_childcount(eq_body) != 1) ||
    (ast_id(ast_child(eq_body)) != TK_IS) ||
    (ast_id(ast_child(ast_child(eq_body))) != TK_THIS) ||
    (ast_id(ast_childidx(ast_child(eq_body), 1)) != TK_PARAMREF))
    return false;

  const char* that_param_name =
    ast_name(ast_child(ast_childidx(ast_child(eq_body), 1)));

  if(ast_name(ast_child(ast_child(eq_params))) != that_param_name)
    return false;

  return true;
}

// Returns 1 for true, 0 for false, -1 if not a Bool literal
static int get_bool_literal_value(ast_t* case_expr)
{
  while(ast_id(case_expr) == TK_SEQ && ast_childcount(case_expr) == 1)
    case_expr = ast_child(case_expr);

  switch(ast_id(case_expr))
  {
    case TK_TRUE: return 1;
    case TK_FALSE: return 0;
    default: return -1;
  }
}

/**
 * return a pointer to the case expr at which the match can be considered
 * exhaustive or NULL if it is not exhaustive.
 **/
static ast_t* is_match_exhaustive(pass_opt_t* opt, ast_t* expr_type,
  ast_t* cases)
{
  pony_assert(expr_type != NULL);
  pony_assert(ast_id(cases) == TK_CASES);

  // Exhaustive match not yet supported for matches where all cases "jump away".
  // The return/error/break/continue should be moved to outside the match.
  if(ast_checkflag(cases, AST_FLAG_JUMPS_AWAY))
    return NULL;

  // Construct a union of all pattern types that count toward exhaustive match.
  ast_t* cases_union_type = ast_from(cases, TK_UNIONTYPE);
  ast_t* result = NULL;
  bool seen_true = false;
  bool seen_false = false;

  for(ast_t* c = ast_child(cases); c != NULL; c = ast_sibling(c))
  {
    AST_GET_CHILDREN(c, case_expr, guard, case_body);
    ast_t* case_type = ast_type(case_expr);

    // if any case is a `_` we have an exhaustive match
    // and we can shortcut here
    if(ast_id(case_expr) == TK_DONTCAREREF)
    {
      result = c;
      break;
    }
    // Only cases with no guard clause can count toward exhaustive match,
    // because the truth of a guard clause can't be statically evaluated.
    // So, for the purposes of exhaustive match, we ignore those cases.
    if(ast_id(guard) != TK_NONE)
      continue;

    // Check for Bool literal patterns (only if no guard).
    // Bool is a machine word, so case_expr_matches_type_alone will return
    // false, but we can still track if both true and false are matched.
    int bool_val = get_bool_literal_value(case_expr);
    if(bool_val == 1) seen_true = true;
    else if(bool_val == 0) seen_false = true;

    // Only cases that match on type alone can count toward exhaustive match,
    // because matches on structural equality can't be statically evaluated.
    // So, for the purposes of exhaustive match, we ignore those cases.
    if(!case_expr_matches_type_alone(opt, case_expr))
      continue;

    // It counts, so add this pattern type to our running union type.
    ast_add(cases_union_type, case_type);

    // If our cases types union is a supertype of the match expression type,
    // then the match must be exhaustive, because all types are covered by cases.
    if(is_subtype(expr_type, cases_union_type, NULL, opt))
    {
      result = c;
      break;
    }
  }

  // If we've seen both true and false literals without guards,
  // Bool is exhaustively covered. Add Bool to union and recheck.
  if(result == NULL && seen_true && seen_false)
  {
    ast_t* bool_type = type_builtin(opt, cases, "Bool");
    ast_add(cases_union_type, bool_type);

    if(is_subtype(expr_type, cases_union_type, NULL, opt))
      result = ast_childlast(cases);  // Point to last case
  }

  ast_free_unattached(cases_union_type);
  return result;
}


bool expr_match(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_MATCH);
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

  if(is_bare(expr_type))
  {
    ast_error(opt->check.errors, expr,
      "a match operand cannot have a bare type");
    ast_error_continue(opt->check.errors, expr_type,
      "type is %s", ast_print_type(expr_type));
    return false;
  }

  ast_t* type = NULL;

  if(!ast_checkflag(cases, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(cases)))
      return false;

    type = control_type_add_branch(opt, type, cases);
  }

  // If the method definition containing the match site had its body inherited
  // from a trait, we don't want to check exhaustiveness here -
  // it should only be checked in the context of the original trait.
  bool skip_exhaustive = false;
  ast_t* body_donor = (ast_t*)ast_data(opt->check.frame->method);
  if ((body_donor != NULL) && (ast_id(body_donor) == TK_TRAIT)
    && (opt->check.frame->type != body_donor))
  {
    skip_exhaustive = true;
  }

  if(!skip_exhaustive)
  {
    // analyze exhaustiveness
    ast_t* exhaustive_at = is_match_exhaustive(opt, expr_type, cases);

    if(exhaustive_at == NULL)
    {
      // match might not be exhaustive
      if ((ast_id(else_clause) == TK_NONE))
      {
        if(ast_has_annotation(ast, "exhaustive"))
        {
          ast_error(opt->check.errors, ast,
            "match marked \\exhaustive\\ is not exhaustive");
          return false;
        }

        // If we have no else clause, and the match is not found to be exhaustive,
        // we must generate an implicit else clause that returns None as the value.
        ast_scope(else_clause);
        ast_setid(else_clause, TK_SEQ);

        BUILD(ref, else_clause,
          NODE(TK_TYPEREF,
            NONE
            ID("None")
            NONE));
        ast_add(else_clause, ref);

        if(!expr_typeref(opt, &ref) || !expr_seq(opt, else_clause))
          return false;
      }
    }
    else
    {
      // match is exhaustive
      if(ast_sibling(exhaustive_at) != NULL)
      {
        // we have unreachable cases
        ast_error(opt->check.errors, ast, "match contains unreachable cases");
        ast_error_continue(opt->check.errors, ast_sibling(exhaustive_at),
          "first unreachable case expression");
        return false;
      }
      else if((ast_id(else_clause) != TK_NONE))
      {
        ast_error(opt->check.errors, ast,
          "match is exhaustive, the else clause is unreachable");
        ast_error_continue(opt->check.errors, else_clause,
          "unreachable code");
        return false;
      }
    }
  }

  if((ast_id(else_clause) != TK_NONE))
  {
    if (!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
    {
      if(is_typecheck_error(ast_type(else_clause)))
        return false;

      type = control_type_add_branch(opt, type, else_clause);
    }
  }

  if((type == NULL) && (ast_sibling(ast) != NULL))
  {
    ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
    return false;
  }

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  return true;
}

bool expr_cases(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_CASES);
  ast_t* the_case = ast_child(ast);
  pony_assert(the_case != NULL);

  ast_t* type = NULL;

  while(the_case != NULL)
  {
    AST_GET_CHILDREN(the_case, pattern, guard, body);
    ast_t* body_type = ast_type(body);

    if(!is_typecheck_error(body_type) &&
      !ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
      type = control_type_add_branch(opt, type, body);

    the_case = ast_sibling(the_case);
  }

  ast_settype(ast, type);
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

  if(ast_checkflag(pattern, AST_FLAG_JUMPS_AWAY))
  {
    ast_error(opt->check.errors, pattern,
      "not a matchable pattern - the expression jumps away with no value");
    return NULL;
  }

  ast_t* pattern_type = ast_type(pattern);

  if(is_typecheck_error(pattern_type))
    return NULL;

  if(is_bare(pattern_type))
  {
    ast_error(opt->check.errors, pattern,
      "a match pattern cannot have a bare type");
    ast_error_continue(opt->check.errors, pattern_type,
      "type is %s", ast_print_type(pattern_type));
    return NULL;
  }

  switch(ast_id(pattern))
  {
    case TK_DONTCAREREF:
    case TK_MATCH_CAPTURE:
    case TK_MATCH_DONTCARE:
      return pattern_type;

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
  deferred_reification_t* fun = lookup(opt, pattern, pattern_type,
    stringtab("eq"));

  if(fun == NULL)
  {
    ast_error(opt->check.errors, pattern,
      "this pattern element doesn't support structural equality");

    return NULL;
  }

  if(ast_id(fun->ast) != TK_FUN)
  {
    ast_error(opt->check.errors, pattern,
      "eq is not a function on this pattern element");
    ast_error_continue(opt->check.errors, fun->ast,
      "definition of eq is here");
    deferred_reify_free(fun);
    return NULL;
  }

  ast_t* r_fun = deferred_reify_method_def(fun, fun->ast, opt);

  AST_GET_CHILDREN(r_fun, cap, id, typeparams, params, result, partial);
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

  ast_t* r_type = set_cap_and_ephemeral(pattern_type, ast_id(cap), TK_EPHEMERAL);
  errorframe_t info = NULL;

  if(!is_subtype(pattern_type, r_type, &info, opt))
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
  ast_free_unattached(r_fun);
  deferred_reify_free(fun);

  return pattern_type;
}

// Infer the types of any literals in the pattern of the given case
static bool infer_pattern_type(ast_t** astp, ast_t* match_expr_type,
  pass_opt_t* opt)
{
  pony_assert(astp != NULL);
  pony_assert(*astp != NULL);
  pony_assert(match_expr_type != NULL);

  if(is_type_literal(match_expr_type))
  {
    ast_error(opt->check.errors, match_expr_type,
      "cannot infer type for literal match expression");
    return false;
  }

  return coerce_literals(astp, match_expr_type, opt);
}

// Check if a type has a capability that would change under aliasing,
// meaning it requires an ephemeral discriminee for a sound capture.
static bool capture_needs_ephemeral(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* cap = cap_fetch(type);
      ast_t* eph = ast_sibling(cap);

      // Already aliased or ephemeral — aliasing won't change this type.
      if(ast_id(eph) != TK_NONE)
        return false;

      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
        case TK_NONE:
          // TK_NONE cap occurs on type param refs with no explicit constraint.
          return true;

        default:
          return false;
      }
    }

    case TK_ARROW:
      return capture_needs_ephemeral(ast_childidx(type, 1));

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(capture_needs_ephemeral(child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return false;

      bool result = capture_needs_ephemeral(unfolded);
      ast_free_unattached(unfolded);
      return result;
    }

    default:
      return false;
  }
}

// Check if a type has any ephemeral component.
// For unions/intersections, "any" is correct: a match capture binds a
// single member, so it's sound if the matched member is ephemeral.
// Tuples are handled position-by-position in check_capture_soundness.
static bool type_is_ephemeral(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* eph = ast_sibling(cap_fetch(type));
      return ast_id(eph) == TK_EPHEMERAL;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(type_is_ephemeral(child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ARROW:
      return type_is_ephemeral(ast_childidx(type, 1));

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return false;

      bool result = type_is_ephemeral(unfolded);
      ast_free_unattached(unfolded);
      return result;
    }

    default:
      return false;
  }
}

// Check that match captures don't grant capabilities that require consuming
// the match expression. This catches viewpoint-adapted and generic captures
// (e.g. this->B iso, this->T where T could be iso) that bypass the
// is_matchtype_with_consumed_pattern check due to viewpoint adaptation
// erasing the ephemeral distinction.
static bool check_capture_soundness(pass_opt_t* opt, ast_t* pattern,
  ast_t* match_type, ast_t* match_expr)
{
  // Unfold any top-level type alias in the match type so the pattern walk
  // below can see the underlying form. This is required for tuple patterns
  // matched against an aliased tuple type, and avoids reporting the alias
  // name when the underlying form is what determines soundness.
  if(ast_id(match_type) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(match_type);

    if(unfolded == NULL)
      return true;

    bool result = check_capture_soundness(opt, pattern, unfolded, match_expr);
    ast_free_unattached(unfolded);
    return result;
  }

  switch(ast_id(pattern))
  {
    case TK_MATCH_CAPTURE:
    {
      ast_t* capture_type = ast_type(pattern);

      if(capture_needs_ephemeral(capture_type) &&
        !type_is_ephemeral(match_type))
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, pattern,
          "this capture is unsound: the capture type can grant capabilities "
          "that require consuming the match expression");
        ast_error_frame(&frame, pattern, "capture type: %s",
          ast_print_type(capture_type));
        ast_error_frame(&frame, match_expr, "match type: %s",
          ast_print_type(match_type));
        ast_error_frame(&frame, pattern,
          "if you need to capture with this capability, "
          "consume the match expression");
        errorframe_report(&frame, opt->check.errors);
        return false;
      }

      return true;
    }

    case TK_SEQ:
    {
      if(ast_childcount(pattern) == 1)
        return check_capture_soundness(opt, ast_child(pattern),
          match_type, match_expr);

      return true;
    }

    case TK_TUPLE:
    {
      if(ast_id(match_type) != TK_TUPLETYPE)
        return true;

      ast_t* pattern_child = ast_child(pattern);
      ast_t* type_child = ast_child(match_type);
      bool ok = true;

      while((pattern_child != NULL) && (type_child != NULL))
      {
        if(!check_capture_soundness(opt, pattern_child,
          type_child, match_expr))
        {
          ok = false;
        }

        pattern_child = ast_sibling(pattern_child);
        type_child = ast_sibling(type_child);
      }

      return ok;
    }

    default:
      return true;
  }
}

bool expr_case(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(opt != NULL);
  pony_assert(ast_id(ast) == TK_CASE);
  AST_GET_CHILDREN(ast, pattern, guard, body);

  if((ast_id(pattern) == TK_NONE) && (ast_id(guard) == TK_NONE))
  {
    ast_error(opt->check.errors, ast,
      "can't have a case with no conditions, use an else clause");
    return false;
  }

  if((ast_id(pattern) == TK_DONTCAREREF))
  {
    ast_error(opt->check.errors, pattern,
      "can't have a case with `_` only, use an else clause");
    return false;
  }

  ast_t* cases = ast_parent(ast);
  ast_t* match = ast_parent(cases);
  ast_t* match_expr = ast_child(match);
  ast_t* match_type = ast_type(match_expr);

  if(ast_checkflag(match_expr, AST_FLAG_JUMPS_AWAY) ||
    is_typecheck_error(match_type))
    return false;

  if(!infer_pattern_type(&pattern, match_type, opt))
    return false;

  ast_t* pattern_type = make_pattern_type(opt, pattern);

  if(pattern_type == NULL)
    return false;

  ast_settype(ast, pattern_type);

   // If the method definition containing the match site had its body inherited
  // from a trait, we don't want to check it here -
  // it should only be checked in the context of the original trait.
  bool skip = false;
  ast_t* body_donor = (ast_t*)ast_data(opt->check.frame->method);
  if ((body_donor != NULL) && (ast_id(body_donor) == TK_TRAIT)
    && (opt->check.frame->type != body_donor))
  {
    skip = true;
  }

  bool ok = true;

  if (!skip)
  {
    errorframe_t info = NULL;

    switch(is_matchtype_with_consumed_pattern(match_type, pattern_type, &info, opt))
    {
      case MATCHTYPE_ACCEPT:
        if(!check_capture_soundness(opt, pattern, match_type, match_expr))
          ok = false;
        break;

      case MATCHTYPE_REJECT:
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, pattern, "this pattern can never match");
        ast_error_frame(&frame, match_type, "match type: %s",
          ast_print_type(match_type));
        // TODO report unaliased type when body is consume !
        ast_error_frame(&frame, pattern, "pattern type: %s",
          ast_print_type(pattern_type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);

        ok = false;
        break;
      }

      case MATCHTYPE_DENY_CAP:
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, pattern,
          "this capture violates capabilities, because the match would "
          "need to differentiate by capability at runtime instead of matching "
          "on type alone");
        ast_error_frame(&frame, match_type, "the match type allows for more than "
          "one possibility with the same type as pattern type, but different "
          "capabilities. match type: %s",
          ast_print_type(match_type));
        ast_error_frame(&frame, pattern, "pattern type: %s",
          ast_print_type(pattern_type));
        errorframe_append(&frame, &info);
        ast_error_frame(&frame, match_expr,
          "the match expression with the inadequate capability is here");
        errorframe_report(&frame, opt->check.errors);

        ok = false;
        break;
      }

      case MATCHTYPE_DENY_NODESC:
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, pattern,
          "this capture cannot match, since the type %s "
          "is a struct and lacks a type descriptor",
          ast_print_type(pattern_type));
        ast_error_frame(&frame, match_type,
          "a struct cannot be part of a union type. match type: %s",
          ast_print_type(match_type));
        ast_error_frame(&frame, pattern, "pattern type: %s",
          ast_print_type(pattern_type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
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
      }
    }
  }
  return ok;
}

bool expr_match_capture(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  pony_assert(ast != NULL);

  AST_GET_CHILDREN(ast, id, type);

  if(is_name_dontcare(ast_name(id)))
    ast_setid(ast, TK_MATCH_DONTCARE);

  pony_assert(type != NULL);

  // Capture type is as specified.
  ast_settype(ast, type);
  ast_settype(ast_child(ast), type);
  return true;
}
