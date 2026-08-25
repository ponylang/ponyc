#include "matchtype.h"
#include "cap.h"
#include "assemble.h"
#include "reify.h"
#include "subtype.h"
#include "typealias.h"
#include "typeparam.h"
#include "viewpoint.h"
#include "ponyassert.h"
#include "alias.h"

static matchtype_t is_x_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt);

static matchtype_t is_union_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_REJECT;

  for(ast_t* child = ast_child(operand);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(child, pattern, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        // If any type in the operand union accepts a match, then the entire
        // operand union accepts a match.
        ok = MATCHTYPE_ACCEPT;
        break;

      case MATCHTYPE_REJECT:
        break;

      // If any type in the operand union denies a match, then the entire
      // operand union is denied a match.
      case MATCHTYPE_DENY_CAP:
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(operand);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(child, pattern, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "no element of %s can match %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_isect_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_ACCEPT;

  for(ast_t* child = ast_child(operand);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(child, pattern, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        // If any type in the operand isect rejects a match, then the entire
        // operand isect rejects match.
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY_CAP:
        // If any type in the operand isect denies a match, then the entire
        // operand isect is denied a match.
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(operand);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(child, pattern, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "not every element of %s can match %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_x_match_union(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_REJECT;

  for(ast_t* child = ast_child(pattern);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(operand, child, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        // If any type in the pattern union accepts a match, the entire pattern
        // union accepts a match.
        ok = MATCHTYPE_ACCEPT;
        break;

      case MATCHTYPE_REJECT:
        break;

      case MATCHTYPE_DENY_CAP:
        // If any type in the pattern union denies a match, the entire pattern
        // union denies a match.
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(pattern);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(operand, child, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "%s cannot match any element of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_x_match_isect(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_ACCEPT;

  for(ast_t* child = ast_child(pattern);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(operand, child, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        // If any type in the pattern isect rejects a match, the entire pattern
        // isect rejects a match.
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY_CAP:
        // If any type in the pattern isect denies a match, the entire pattern
        // isect denies a match.
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(pattern);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(operand, child, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "%s cannot match every element of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_tuple_match_tuple(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // Must be a pairwise match.
  if(ast_childcount(operand) != ast_childcount(pattern))
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: they have a different number of elements",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  ast_t* operand_child = ast_child(operand);
  ast_t* pattern_child = ast_child(pattern);
  matchtype_t ok = MATCHTYPE_ACCEPT;

  while(operand_child != NULL)
  {
    switch(is_x_match_x(operand_child, pattern_child, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY_CAP:
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if(ok != MATCHTYPE_ACCEPT)
      break;

    operand_child = ast_sibling(operand_child);
    pattern_child = ast_sibling(pattern_child);
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    operand_child = ast_child(operand);
    pattern_child = ast_child(pattern);

    while(operand_child != NULL)
    {
      is_x_match_x(operand_child, pattern_child, errorf, report_reject, opt);

      operand_child = ast_sibling(operand_child);
      pattern_child = ast_sibling(pattern_child);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "%s cannot pairwise match %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_nominal_match_tuple(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  if(!is_top_type(operand, true, opt))
  {
    if((errorf != NULL) && report_reject)
    {
      ast_t* operand_def = (ast_t*)ast_data(operand);

      ast_error_frame(errorf, pattern,
        "%s cannot match %s: the pattern type is a tuple",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      ast_error_frame(errorf, operand_def, "this might be possible if the "
        "match type were an empty interface, such as the Any type");
    }

    return MATCHTYPE_REJECT;
  }

  ast_t* child = ast_child(pattern);

  while(child != NULL)
  {
    matchtype_t r = is_x_match_x(operand, child, errorf, false, opt);
    pony_assert(r != MATCHTYPE_REJECT);

    if(r == MATCHTYPE_DENY_CAP)
    {
      if(errorf != NULL)
      {
        ast_error_frame(errorf, pattern,
          "matching %s with %s could violate capabilities",
          ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      }

      return r;
    } else if (r == MATCHTYPE_DENY_NODESC) {
      if(errorf != NULL)
      {
        ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      }

      return r;
    }

    child = ast_sibling(child);
  }

  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_typeparam_match_typeparam(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  (void)opt;
  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  // Dig through defs if there are multiple layers of directly-bound
  // type params (created through the collect_type_params function).
  operand_def = typeparam_root(operand_def);
  pattern_def = typeparam_root(pattern_def);

  ast_t* o_cap = cap_fetch(operand);
  ast_t* o_eph = ast_sibling(o_cap);
  ast_t* p_cap = cap_fetch(pattern);
  ast_t* p_eph = ast_sibling(p_cap);

  matchtype_t r = MATCHTYPE_REJECT;

  if(operand_def == pattern_def)
  {
    r = is_cap_sub_cap_bound(ast_id(o_cap), TK_EPHEMERAL,
      ast_id(p_cap), ast_id(p_eph)) ? MATCHTYPE_ACCEPT : MATCHTYPE_DENY_CAP;
  }

  if((r != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if(r == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a bound subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));
    } else if (r == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: they are different type parameters",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return r;
}

static matchtype_t is_typeparam_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  if(ast_id(pattern) == TK_TYPEPARAMREF)
  {
    matchtype_t ok = is_typeparam_match_typeparam(operand, pattern, errorf,
      false, opt);

    if(ok != MATCHTYPE_REJECT)
      return ok;
  }

  ast_t* operand_upper = typeparam_upper(operand);

  // An unconstrained typeparam could match anything.
  if(operand_upper == NULL)
    return MATCHTYPE_ACCEPT;

  // Check if the constraint can match the pattern.
  matchtype_t ok = is_x_match_x(operand_upper, pattern, errorf, report_reject,
    opt);
  ast_free_unattached(operand_upper);
  return ok;
}

static matchtype_t is_arrow_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // upperbound(this->T1) match T2
  // ---
  // (this->T1) match T2

  // lowerbound(T1->T2) match T3
  // ---
  // (T1->T2) match T3

  ast_t* operand_view;

  AST_GET_CHILDREN(operand, left, right);

  if(ast_id(left) == TK_THISTYPE)
    operand_view = viewpoint_upper(operand, opt);
  else
    operand_view = viewpoint_lower(operand, opt);

  if(operand_view == NULL)
  {
    if(errorf != NULL)
    {
      // this->X always has an upper bound.
      pony_assert(ast_id(left) != TK_THISTYPE);

      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "the match type has no lower bounds",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_DENY_CAP;
  }

  matchtype_t ok = is_x_match_x(operand_view, pattern, errorf, report_reject,
    opt);
  ast_free_unattached(operand_view);
  return ok;
}

static matchtype_t is_x_match_tuple(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
      return is_tuple_match_tuple(operand, pattern, errorf, report_reject, opt);

    case TK_NOMINAL:
      return is_nominal_match_tuple(operand, pattern, errorf, report_reject,
        opt);

    case TK_TYPEPARAMREF:
      return is_typeparam_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(operand);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;
      matchtype_t ok = is_x_match_tuple(unfolded, pattern, errorf,
        report_reject, opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static bool contains_typeparam(ast_t* ast)
{
  if(ast_id(ast) == TK_TYPEPARAMREF)
    return true;

  if(ast_id(ast) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(ast);
    if(unfolded == NULL)
      return false;

    bool r = contains_typeparam(unfolded);
    if(unfolded != ast)
      ast_free_unattached(unfolded);
    return r;
  }

  ast_t* child = ast_child(ast);
  while(child != NULL)
  {
    if(contains_typeparam(child))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

// Does the type parameter identified by tp_def appear anywhere inside
// other's structure, under a generative type constructor (a nominal,
// a tuple, or an arrow)?
//
// A pair like A vs Wrap[A] cannot unify: no substitution for A satisfies
// A = Wrap[A] without an infinite type. Direct occurrences in a union or
// intersection arm do NOT count — A vs (A | U8) is satisfied by any
// reification with U8 ⊆ A (e.g. A = (U8 | I32) makes (A | U8) = A).
// under_wrapper becomes true when the walk descends into a generative
// constructor. Canonicalizes each typeparamref def through typeparam_root
// — a raw def pointer may be one layer in a chain built by
// collect_type_params.
static bool typeparam_occurs_in(ast_t* tp_def, ast_t* other,
  bool under_wrapper)
{
  if(ast_id(other) == TK_TYPEPARAMREF)
  {
    if(!under_wrapper)
      return false;

    ast_t* other_def = typeparam_root((ast_t*)ast_data(other));
    return other_def == tp_def;
  }

  if(ast_id(other) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(other);
    if(unfolded == NULL)
      return false;

    bool r = typeparam_occurs_in(tp_def, unfolded, under_wrapper);
    if(unfolded != other)
      ast_free_unattached(unfolded);
    return r;
  }

  token_id id = ast_id(other);
  bool descend_wrapped = under_wrapper
    || (id == TK_NOMINAL) || (id == TK_TUPLETYPE) || (id == TK_ARROW);

  ast_t* child = ast_child(other);
  while(child != NULL)
  {
    if(typeparam_occurs_in(tp_def, child, descend_wrapped))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

// Could two type-argument positions reify to equal types for some
// substitution of the type parameters they contain?
//
// The is_eqtype fast path is cap-aware. The type-parameter fallback
// deliberately ignores capabilities: the outer entity descriptor identity
// check at runtime discriminates on the fully reified type arguments'
// caps, so a compile-time cap check on the pair itself would spuriously
// reject matches the runtime would accept.
//
//   - eqtype pairs unify.
//   - a bare type parameter unifies with anything the other side satisfies
//     as a reification of the parameter's constraint (subtype, ignoring
//     caps); an unconstrained parameter unifies with anything. The
//     parameter's def must not appear inside a generative constructor on
//     the other side (occurs check): no reification satisfies A = f(A)
//     without an infinite type. Occurrences in a union or intersection
//     arm are not generative — A vs (A | U8) can unify.
//   - two type parameters unify when their constraints share any type
//     (approximated as one constraint being a subtype of the other; two
//     constraints that only overlap through a common subtype are not
//     recognized here — see follow-up work).
//   - two same-definition nominals unify when every pair of type arguments
//     recursively unifies.
//   - a type alias reference is unfolded and re-tried.
//   - anything else does not unify.
static bool typeargs_could_unify(ast_t* a, ast_t* b, pass_opt_t* opt)
{
  if(is_eqtype(a, b, NULL, opt))
    return true;

  if(ast_id(a) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(a);
    if(unfolded == NULL)
      return false;

    bool r = typeargs_could_unify(unfolded, b, opt);
    if(unfolded != a)
      ast_free_unattached(unfolded);
    return r;
  }

  if(ast_id(b) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(b);
    if(unfolded == NULL)
      return false;

    bool r = typeargs_could_unify(a, unfolded, opt);
    if(unfolded != b)
      ast_free_unattached(unfolded);
    return r;
  }

  bool a_tp = ast_id(a) == TK_TYPEPARAMREF;
  bool b_tp = ast_id(b) == TK_TYPEPARAMREF;

  if(a_tp && b_tp)
  {
    ast_t* a_up = typeparam_upper(a);
    ast_t* b_up = typeparam_upper(b);
    bool r;

    if((a_up == NULL) || (b_up == NULL))
      r = true;
    else
      r = is_subtype_ignore_cap(a_up, b_up, NULL, opt)
       || is_subtype_ignore_cap(b_up, a_up, NULL, opt);

    if(a_up != NULL) ast_free_unattached(a_up);
    if(b_up != NULL) ast_free_unattached(b_up);
    return r;
  }

  if(a_tp || b_tp)
  {
    ast_t* tp = a_tp ? a : b;
    ast_t* other = a_tp ? b : a;

    ast_t* tp_def = typeparam_root((ast_t*)ast_data(tp));
    if(typeparam_occurs_in(tp_def, other, false))
      return false;

    ast_t* upper = typeparam_upper(tp);
    if(upper == NULL)
      return true;

    bool r = is_subtype_ignore_cap(other, upper, NULL, opt);
    ast_free_unattached(upper);
    return r;
  }

  if((ast_id(a) == TK_NOMINAL) && (ast_id(b) == TK_NOMINAL))
  {
    ast_t* a_def = (ast_t*)ast_data(a);
    ast_t* b_def = (ast_t*)ast_data(b);

    if(a_def != b_def)
      return false;

    ast_t* a_typeargs = ast_childidx(a, 2);
    ast_t* b_typeargs = ast_childidx(b, 2);

    ast_t* a_arg = ast_child(a_typeargs);
    ast_t* b_arg = ast_child(b_typeargs);

    while((a_arg != NULL) && (b_arg != NULL))
    {
      if(!typeargs_could_unify(a_arg, b_arg, opt))
        return false;

      a_arg = ast_sibling(a_arg);
      b_arg = ast_sibling(b_arg);
    }

    return (a_arg == NULL) && (b_arg == NULL);
  }

  return false;
}

// Pairwise match of two type-argument lists for same-definition nominals.
//
// For each pair:
//   - is_eqtype: the pair matches.
//   - both sides fully concrete but not eqtype: reject.
//   - a type parameter appears in either side:
//       - struct_pattern: deny_nodesc. A struct has no runtime descriptor,
//         so the type argument cannot be checked; the runtime would treat
//         any match as unconditional.
//       - otherwise: check whether the pair could unify under some
//         reification of the type parameters.
//
// A deny_nodesc from any pair propagates; a single failing pair otherwise
// rejects the whole list.
static matchtype_t match_typeargs_pairwise(ast_t* o_typeargs, ast_t* p_typeargs,
  bool struct_pattern, pass_opt_t* opt)
{
  ast_t* o_arg = ast_child(o_typeargs);
  ast_t* p_arg = ast_child(p_typeargs);

  matchtype_t ok = MATCHTYPE_ACCEPT;

  while((o_arg != NULL) && (p_arg != NULL))
  {
    matchtype_t pair;

    if(is_eqtype(o_arg, p_arg, NULL, opt))
      pair = MATCHTYPE_ACCEPT;
    else if(!contains_typeparam(o_arg) && !contains_typeparam(p_arg))
      pair = MATCHTYPE_REJECT;
    else if(struct_pattern)
      pair = MATCHTYPE_DENY_NODESC;
    else if(typeargs_could_unify(o_arg, p_arg, opt))
      pair = MATCHTYPE_ACCEPT;
    else
      pair = MATCHTYPE_REJECT;

    if(pair == MATCHTYPE_DENY_NODESC)
      return MATCHTYPE_DENY_NODESC;

    if((pair == MATCHTYPE_REJECT) && (ok == MATCHTYPE_ACCEPT))
      ok = MATCHTYPE_REJECT;

    o_arg = ast_sibling(o_arg);
    p_arg = ast_sibling(p_arg);
  }

  return ok;
}

static matchtype_t is_nominal_match_entity(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  // We say the pattern provides the operand if it is a subtype without taking
  // capabilities into account.
  bool provides = is_subtype_ignore_cap(pattern, operand, NULL, opt);

  // If the strict subtype check rejects but the operand and pattern share
  // the same entity definition, the rejection may be caused solely by type
  // arguments that cannot be proved equivalent when a type parameter is
  // involved. Re-check the type arguments pairwise; a type parameter can
  // reify to make the pair equal at runtime, and a same-definition entity
  // pattern is discriminated at runtime by descriptor identity of the fully
  // reified type. See #723.
  if(!provides)
  {
    ast_t* operand_def = (ast_t*)ast_data(operand);
    ast_t* pattern_def = (ast_t*)ast_data(pattern);

    if(operand_def == pattern_def
      && match_typeargs_pairwise(o_typeargs, p_typeargs, false, opt)
        == MATCHTYPE_ACCEPT)
    {
      provides = true;
    }
  }

  // If the pattern doesn't provide the operand, reject the match.
  if(!provides)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: %s isn't a subtype of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type_no_cap(pattern, opt->strtab), ast_print_type_no_cap(operand, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  // If the operand does provide the pattern, but the operand refcap can't
  // match the pattern refcap, deny the match.
  if(!is_cap_sub_cap(ast_id(o_cap), ast_id(o_eph),
    ast_id(p_cap), ast_id(p_eph)))
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));

        if(is_cap_sub_cap(ast_id(o_cap), TK_EPHEMERAL, ast_id(p_cap),
          ast_id(p_eph)))
          ast_error_frame(errorf, o_cap,
            "this would be possible if the subcap were more ephemeral. "
            "Perhaps you meant to consume this variable");
  }

    return MATCHTYPE_DENY_CAP;
  }

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_nominal_match_struct(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // A struct pattern can only be used if the operand is the same struct.
  // Otherwise, since there is no type descriptor, there is no way to
  // determine a match at runtime.
  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  // This must be a deny to prevent a union or intersection type that includes
  // the same struct as the pattern from matching.
  if(operand_def != pattern_def)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: the pattern type is a struct",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      ast_error_frame(errorf, pattern,
        "since a struct has no type descriptor, pattern matching at runtime "
        "would be impossible");
    }

    return MATCHTYPE_DENY_NODESC;
  }

  // Same-definition struct: a type parameter inside a type argument cannot
  // be discriminated at runtime (a struct has no descriptor), so accepting
  // the pair would make the runtime match unconditional. Deny here; the
  // concrete-argument cases fall through to the entity path, which reports
  // reject or accept as it does for any other same-definition nominal.
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  if(match_typeargs_pairwise(o_typeargs, p_typeargs, true, opt)
    == MATCHTYPE_DENY_NODESC)
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type "
        "descriptor",
        ast_print_type(operand, opt->strtab),
        ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_DENY_NODESC;
  }

  return is_nominal_match_entity(operand, pattern, errorf, report_reject, opt);
}

// Same-def match in an open-ended provides walk: the operand's declared
// provides list is followed transitively; at any reified provided nominal
// whose def matches the pattern's, the type-argument pair is checked with
// match_typeargs_pairwise, which accepts when a type parameter could reify
// to make the pair equal. See #5859.
//
// Handles nominal provides only. Structural interface satisfaction (a
// class that satisfies an interface without declaring `is I`) is left to
// follow-up work.
//
// The walk terminates because Pony rejects cyclic trait/interface provides
// declarations at compile time ("traits and interfaces can't be recursive"),
// so the provides DAG the walk follows is finite.
static bool provides_could_match_pattern(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  if(ast_id(operand) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(operand);
    if(unfolded == NULL)
      return false;
    bool r = provides_could_match_pattern(unfolded, pattern, opt);
    if(unfolded != operand)
      ast_free_unattached(unfolded);
    return r;
  }

  if(ast_id(operand) != TK_NOMINAL)
    return false;

  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);
  ast_t* o_typeargs = ast_childidx(operand, 2);
  ast_t* p_typeargs = ast_childidx(pattern, 2);

  if(operand_def == pattern_def)
  {
    return match_typeargs_pairwise(o_typeargs, p_typeargs, false, opt)
      == MATCHTYPE_ACCEPT;
  }

  AST_GET_CHILDREN(operand_def, o_id, o_typeparams, o_defcap, o_provides);
  ast_t* child = ast_child(o_provides);
  while(child != NULL)
  {
    ast_t* r_child = reify(child, o_typeparams, o_typeargs, opt, true);
    pony_assert(r_child != NULL);

    bool matched = provides_could_match_pattern(r_child, pattern, opt);

    if(r_child != child)
      ast_free_unattached(r_child);

    if(matched)
      return true;

    child = ast_sibling(child);
  }

  return false;
}

static matchtype_t is_entity_match_trait(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  bool provides = is_subtype_ignore_cap(operand, pattern, NULL, opt);

  // If the strict subtype check rejects, the rejection may be caused solely
  // by type arguments that cannot be proved equivalent when a type parameter
  // is involved. Walk the operand's `provides` transitively: at any reified
  // provided type that shares its def with the pattern, a type parameter
  // could reify to make the pair equal at runtime, and the reachable-trait
  // bitmap discriminates on the fully reified type. See #5859.
  //
  // Struct operands are excluded: is_struct_sub_trait denies unconditionally
  // because a struct has no descriptor to discriminate a trait/interface
  // match at runtime, and that denial is load-bearing — accepting here would
  // compile a match with no runtime check and can crash on dispatch.
  if(!provides
    && ast_id((ast_t*)ast_data(operand)) != TK_STRUCT
    && provides_could_match_pattern(operand, pattern, opt))
    provides = true;

  // If the operand doesn't provide the pattern (trait or interface), reject
  // the match.
  if(!provides)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: %s isn't a subtype of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type_no_cap(operand, opt->strtab), ast_print_type_no_cap(pattern, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  // If the operand does provide the pattern, but the operand refcap can't
  // match the pattern refcap, deny the match.
  if(!is_cap_sub_cap(ast_id(o_cap), TK_EPHEMERAL,
    ast_id(p_cap), ast_id(p_eph)))
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));
    }

    return MATCHTYPE_DENY_CAP;
  }

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_trait_match_trait(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  (void)report_reject;
  (void)opt;
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  // If the operand refcap can't match the pattern refcap, deny the match.
  if(!is_cap_sub_cap(ast_id(o_cap), TK_EPHEMERAL,
    ast_id(p_cap), ast_id(p_eph)))
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));
    }

    return MATCHTYPE_DENY_CAP;
  }

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_nominal_match_trait(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  ast_t* operand_def = (ast_t*)ast_data(operand);

  switch(ast_id(operand_def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return is_entity_match_trait(operand, pattern, errorf, report_reject,
        opt);

    case TK_TRAIT:
    case TK_INTERFACE:
      return is_trait_match_trait(operand, pattern, errorf, report_reject,
        opt);

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_nominal_match_nominal(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  switch(ast_id(pattern_def))
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return is_nominal_match_entity(operand, pattern, errorf, report_reject,
        opt);

    case TK_STRUCT:
      return is_nominal_match_struct(operand, pattern, errorf, report_reject,
        opt);

    case TK_TRAIT:
    case TK_INTERFACE:
      return is_nominal_match_trait(operand, pattern, errorf, report_reject,
        opt);

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_tuple_match_nominal(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  (void)opt;

  if((errorf != NULL) && report_reject)
  {
    ast_error_frame(errorf, pattern,
      "%s cannot match %s: the match type is a tuple",
      ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
  }

  return MATCHTYPE_REJECT;
}

static matchtype_t is_x_match_nominal(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
      return is_tuple_match_nominal(operand, pattern, errorf, report_reject,
        opt);

    case TK_NOMINAL:
      return is_nominal_match_nominal(operand, pattern, errorf, report_reject,
        opt);

    case TK_TYPEPARAMREF:
      return is_typeparam_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(operand);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;
      matchtype_t ok = is_x_match_nominal(unfolded, pattern, errorf,
        report_reject, opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
      return MATCHTYPE_REJECT;

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_x_match_base_typeparam(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
    case TK_NOMINAL:
      return MATCHTYPE_REJECT;

    case TK_TYPEPARAMREF:
      return is_typeparam_match_typeparam(operand, pattern, errorf, false, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(operand);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;
      matchtype_t ok = is_x_match_base_typeparam(unfolded, pattern, errorf,
        report_reject, opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
      return MATCHTYPE_REJECT;

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_x_match_typeparam(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = is_x_match_base_typeparam(operand, pattern, errorf,
    report_reject, opt);

  if(ok != MATCHTYPE_REJECT)
    return ok;

  ast_t* pattern_upper = typeparam_upper(pattern);

  // An unconstrained typeparam can match anything.
  if(pattern_upper == NULL)
    return MATCHTYPE_ACCEPT;

  // Otherwise, match the constraint.
  ok = is_x_match_x(operand, pattern_upper, errorf, report_reject, opt);
  ast_free_unattached(pattern_upper);
  return ok;
}

static matchtype_t is_x_match_arrow(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // T1 match upperbound(T2->T3)
  // ---
  // T1 match T2->T3
  ast_t* pattern_upper = viewpoint_upper(pattern, opt);

  if(pattern_upper == NULL)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: the pattern type has no upper bounds",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  matchtype_t ok = is_x_match_x(operand, pattern_upper, errorf, report_reject,
    opt);
  ast_free_unattached(pattern_upper);
  return ok;
}

static matchtype_t is_x_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  if(ast_id(pattern) == TK_DONTCARETYPE)
    return MATCHTYPE_ACCEPT;

  switch(ast_id(pattern))
  {
    case TK_UNIONTYPE:
      return is_x_match_union(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_x_match_isect(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
      return is_x_match_tuple(operand, pattern, errorf, report_reject, opt);

    case TK_NOMINAL:
      return is_x_match_nominal(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEPARAMREF:
      return is_x_match_typeparam(operand, pattern, errorf, report_reject,
        opt);

    case TK_ARROW:
      return is_x_match_arrow(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(pattern);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;

      matchtype_t ok = is_x_match_x(operand, unfolded, errorf, report_reject,
        opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
      return MATCHTYPE_DENY_CAP;

    default:
      pony_assert(0);
      return MATCHTYPE_DENY_CAP;
  }
}

matchtype_t is_matchtype(ast_t* operand, ast_t* pattern, errorframe_t* errorf,
  pass_opt_t* opt)
{
  return is_x_match_x(operand, pattern, errorf, true, opt);
}

matchtype_t is_matchtype_with_consumed_pattern(ast_t* operand, ast_t* pattern, errorframe_t* errorf,
  pass_opt_t* opt)
{
  ast_t* consumed_pattern = consume_type(pattern, TK_NONE, false, opt);
  if (consumed_pattern == NULL)
    return MATCHTYPE_REJECT;

  matchtype_t rslt = is_x_match_x(operand, consumed_pattern, errorf, true, opt);

  // TODO discuss with joe
  if (consumed_pattern != pattern)
    ast_free_unattached(consumed_pattern);

  return rslt;
}
