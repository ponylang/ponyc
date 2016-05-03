#include "matchtype.h"
#include "cap.h"
#include "assemble.h"
#include "subtype.h"
#include "typeparam.h"
#include "viewpoint.h"
#include <assert.h>

static matchtype_t is_x_match_x(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt);

static matchtype_t is_union_match_x(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_REJECT;

  for(ast_t* child = ast_child(operand);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(child, pattern, opt))
    {
      case MATCHTYPE_ACCEPT:
        // If any type in the operand union accepts a match, then the entire
        // operand union accepts a match.
        ok = MATCHTYPE_ACCEPT;
        break;

      case MATCHTYPE_REJECT:
        break;

      case MATCHTYPE_DENY:
        // If any type in the operand union denies a match, then the entire
        // operand union is denied a match.
        return MATCHTYPE_DENY;
    }
  }

  return ok;
}

static matchtype_t is_isect_match_x(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_ACCEPT;

  for(ast_t* child = ast_child(operand);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(child, pattern, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        // If any type in the operand isect rejects a match, then the entire
        // operand isect rejects match.
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY:
        // If any type in the operand isect denies a match, then the entire
        // operand isect is denied a match.
        return MATCHTYPE_DENY;
    }
  }

  return ok;
}

static matchtype_t is_x_match_union(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_REJECT;

  for(ast_t* child = ast_child(pattern);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(operand, child, opt))
    {
      case MATCHTYPE_ACCEPT:
        // If any type in the pattern union accepts a match, the entire pattern
        // union accepts a match.
        ok = MATCHTYPE_ACCEPT;
        break;

      case MATCHTYPE_REJECT:
        break;

      case MATCHTYPE_DENY:
        // If any type in the pattern union denies a match, the entire pattern
        // union denies a match.
        return MATCHTYPE_DENY;
    }
  }

  return ok;
}

static matchtype_t is_x_match_isect(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_ACCEPT;

  for(ast_t* child = ast_child(pattern);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(operand, child, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        // If any type in the pattern isect rejects a match, the entire pattern
        // isect rejects a match.
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY:
        // If any type in the pattern isect denies a match, the entire pattern
        // isect denies a match.
        return MATCHTYPE_DENY;
    }
  }

  return ok;
}

static matchtype_t is_tuple_match_tuple(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  // Must be a pairwise match.
  if(ast_childcount(operand) != ast_childcount(pattern))
    return MATCHTYPE_REJECT;

  ast_t* operand_child = ast_child(operand);
  ast_t* pattern_child = ast_child(pattern);
  matchtype_t ok = MATCHTYPE_ACCEPT;

  while(operand_child != NULL)
  {
    switch(is_x_match_x(operand_child, pattern_child, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY:
        return MATCHTYPE_DENY;
    }

    operand_child = ast_sibling(operand_child);
    pattern_child = ast_sibling(pattern_child);
  }

  return ok;
}

static matchtype_t is_typeparam_match_x(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  ast_t* operand_upper = typeparam_upper(operand);

  // An unconstrained typeparam could match anything.
  if(operand_upper == NULL)
    return MATCHTYPE_ACCEPT;

  // Check if the constraint can match the pattern.
  matchtype_t ok = is_x_match_x(operand_upper, pattern, opt);
  ast_free_unattached(operand_upper);
  return ok;
}

static matchtype_t is_x_match_tuple(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, opt);

    case TK_TUPLETYPE:
      return is_tuple_match_tuple(operand, pattern, opt);

    case TK_NOMINAL:
      return MATCHTYPE_REJECT;

    case TK_TYPEPARAMREF:
      return is_typeparam_match_x(operand, pattern, opt);

    case TK_ARROW:
      return MATCHTYPE_REJECT;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_nominal_match_entity(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  // We say the pattern provides the operand if it is a subtype after changing
  // it to have the same refcap as the operand.
  token_id tcap = ast_id(p_cap);
  token_id teph = ast_id(p_eph);
  ast_t* r_operand = set_cap_and_ephemeral(operand, tcap, teph);
  bool provides = is_subtype(pattern, r_operand, NULL, opt);
  ast_free_unattached(r_operand);

  // If the pattern doesn't provide the operand, reject the match.
  if(!provides)
    return MATCHTYPE_REJECT;

  // If the operand does provide the pattern, but the operand refcap can't
  // match the pattern refcap, deny the match.
  if(!is_cap_match_cap(ast_id(o_cap), ast_id(o_eph), tcap, teph))
    return MATCHTYPE_DENY;

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_entity_match_trait(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  token_id tcap = ast_id(p_cap);
  token_id teph = ast_id(p_eph);
  ast_t* r_operand = set_cap_and_ephemeral(operand, tcap, teph);
  bool provides = is_subtype(r_operand, pattern, NULL, opt);
  ast_free_unattached(r_operand);

  // If the operand doesn't provide the pattern (trait or interface), reject
  // the match.
  if(!provides)
    return MATCHTYPE_REJECT;

  // If the operand does provide the pattern, but the operand refcap can't
  // match the pattern refcap, deny the match.
  if(!is_cap_match_cap(ast_id(o_cap), ast_id(o_eph), tcap, teph))
    return MATCHTYPE_DENY;

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_trait_match_trait(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  (void)opt;
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  // If the operand refcap can't match the pattern refcap, deny the match.
  if(!is_cap_match_cap(ast_id(o_cap), ast_id(o_eph),
    ast_id(p_cap), ast_id(p_eph)))
    return MATCHTYPE_DENY;

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_nominal_match_trait(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  ast_t* operand_def = (ast_t*)ast_data(operand);

  switch(ast_id(operand_def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return is_entity_match_trait(operand, pattern, opt);

    case TK_TRAIT:
    case TK_INTERFACE:
      return is_trait_match_trait(operand, pattern, opt);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_nominal_match_nominal(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  switch(ast_id(pattern_def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return is_nominal_match_entity(operand, pattern, opt);

    case TK_TRAIT:
    case TK_INTERFACE:
      return is_nominal_match_trait(operand, pattern, opt);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_arrow_match_x(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
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
    operand_view = viewpoint_upper(operand);
  else
    operand_view = viewpoint_lower(operand);

  if(operand_view == NULL)
    return MATCHTYPE_DENY;

  matchtype_t ok = is_x_match_x(operand_view, pattern, opt);
  ast_free_unattached(operand_view);
  return ok;
}

static matchtype_t is_x_match_nominal(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, opt);

    case TK_TUPLETYPE:
      return MATCHTYPE_REJECT;

    case TK_NOMINAL:
      return is_nominal_match_nominal(operand, pattern, opt);

    case TK_TYPEPARAMREF:
      return is_typeparam_match_x(operand, pattern, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, opt);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_x_match_typeparam(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  ast_t* pattern_upper = typeparam_upper(pattern);

  // An unconstrained typeparam can match anything.
  if(pattern_upper == NULL)
    return MATCHTYPE_ACCEPT;

  // Otherwise, match the constraint.
  matchtype_t ok = is_x_match_x(operand, pattern_upper, opt);
  ast_free_unattached(pattern_upper);
  return ok;
}

static matchtype_t is_x_match_arrow(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  // T1 match upperbound(T2->T3)
  // ---
  // T1 match T2->T3
  ast_t* pattern_upper = viewpoint_upper(pattern);

  if(pattern_upper == NULL)
    return MATCHTYPE_REJECT;

  matchtype_t ok = is_x_match_x(operand, pattern_upper, opt);
  ast_free_unattached(pattern_upper);
  return ok;
}

static matchtype_t is_x_match_x(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  if(ast_id(pattern) == TK_DONTCARE)
    return MATCHTYPE_ACCEPT;

  switch(ast_id(pattern))
  {
    case TK_UNIONTYPE:
      return is_x_match_union(operand, pattern, opt);

    case TK_ISECTTYPE:
      return is_x_match_isect(operand, pattern, opt);

    case TK_TUPLETYPE:
      return is_x_match_tuple(operand, pattern, opt);

    case TK_NOMINAL:
      return is_x_match_nominal(operand, pattern, opt);

    case TK_TYPEPARAMREF:
      return is_x_match_typeparam(operand, pattern, opt);

    case TK_ARROW:
      return is_x_match_arrow(operand, pattern, opt);

    case TK_DONTCARE:
      return MATCHTYPE_ACCEPT;

    case TK_FUNTYPE:
      return MATCHTYPE_DENY;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

matchtype_t is_matchtype(ast_t* operand, ast_t* pattern, pass_opt_t* opt)
{
  return is_x_match_x(operand, pattern, opt);
}
