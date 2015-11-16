#include "matchtype.h"
#include "subtype.h"
#include "viewpoint.h"
#include "assemble.h"
#include <assert.h>

static matchtype_t is_matchtype_pattern_union(ast_t* operand, ast_t* pattern)
{
  // Some component of pattern must be a possible match with operand.
  matchtype_t r = MATCHTYPE_REJECT;

  for(ast_t* p = ast_child(pattern); p != NULL; p = ast_sibling(p))
  {
    matchtype_t ok = is_matchtype(operand, p);

    if(ok == MATCHTYPE_DENY)
      return MATCHTYPE_DENY;

    if(ok == MATCHTYPE_ACCEPT)
      r = MATCHTYPE_ACCEPT;
  }

  return r;
}

static matchtype_t is_matchtype_pattern_isect(ast_t* operand, ast_t* pattern)
{
  // Every component of pattern must be a possible match with operand.
  matchtype_t r = MATCHTYPE_ACCEPT;

  for(ast_t* p = ast_child(pattern); p != NULL; p = ast_sibling(p))
  {
    matchtype_t ok = is_matchtype(operand, p);

    if(ok == MATCHTYPE_DENY)
      return MATCHTYPE_DENY;

    if(ok == MATCHTYPE_REJECT)
      r = MATCHTYPE_REJECT;
  }

  return r;
}

static matchtype_t is_matchtype_pattern_arrow(ast_t* operand, ast_t* pattern)
{
  // Check the lower bounds of pattern.
  ast_t* lower = viewpoint_lower(pattern);
  matchtype_t ok = is_matchtype(operand, lower);

  if(lower != operand)
    ast_free_unattached(lower);

  return ok;
}

static bool is_matchtype_operand_typearg(ast_t* operand, ast_t* pattern)
{
  token_id toperand = ast_id(operand);
  token_id tpattern = ast_id(pattern);

  if(toperand == TK_TYPEPARAMREF)
    return is_matchtype(operand, pattern) == MATCHTYPE_ACCEPT;

  if(tpattern == TK_TYPEPARAMREF)
    return is_matchtype(pattern, operand) == MATCHTYPE_ACCEPT;

  if((toperand == TK_TUPLETYPE) && (tpattern == TK_TUPLETYPE))
  {
    ast_t* operand_child = ast_child(operand);
    ast_t* pattern_child = ast_child(pattern);

    while((operand_child != NULL) && (pattern_child != NULL))
    {
      if(!is_matchtype_operand_typearg(operand_child, pattern_child))
        return false;

      operand_child = ast_sibling(operand_child);
      pattern_child = ast_sibling(pattern_child);
    }

    if((operand_child != NULL) || (pattern_child != NULL))
      return false;

    return true;
  }

  return is_eqtype(operand, pattern);
}

static matchtype_t is_matchtype_subtype_or_deny(ast_t* operand, ast_t* pattern)
{
  // At this point, operand must be a subtype of pattern for an accept.
  if(is_subtype(operand, pattern))
    return MATCHTYPE_ACCEPT;

  // If operand would be a subtype of pattern if it had pattern's capability
  // and ephemerality, we deny other matches.
  token_id cap, eph;

  switch(ast_id(pattern))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);
      cap = ast_id(p_cap);
      eph = ast_id(p_eph);
      break;
    }

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(pattern, p_id, p_cap, p_eph);
      cap = ast_id(p_cap);
      eph = ast_id(p_eph);
      break;
    }

    default:
      assert(0);
      return MATCHTYPE_DENY;
  }

  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);
  ast_t* r_type = set_cap_and_ephemeral(operand, cap, eph);

  matchtype_t ok = MATCHTYPE_REJECT;

  if(is_subtype(r_type, pattern))
  {
    // Operand would be a subtype of pattern if their capabilities were the
    // same.
    // Deny any match, since this would break capabilities.
    ok = MATCHTYPE_DENY;
  }
  else if(operand_def != pattern_def)
  {
    // Operand and pattern are unrelated types.
    ok = MATCHTYPE_REJECT;
  }
  else if((operand_def == pattern_def) &&
    is_sub_cap_and_ephemeral(operand, pattern))
  {
    // Operand and pattern are the same base type, but have different type
    // args. Only accept if operand's type args are typeparamrefs and they
    // could be a subtype of pattern's type args.
    assert(ast_id(operand) == TK_NOMINAL);
    assert(ast_id(pattern) == TK_NOMINAL);

    AST_GET_CHILDREN(operand, o_pkg, o_id, operand_typeargs);
    AST_GET_CHILDREN(pattern, p_pkg, p_id, pattern_typeargs);

    ast_t* operand_typearg = ast_child(operand_typeargs);
    ast_t* pattern_typearg = ast_child(pattern_typeargs);
    ok = MATCHTYPE_ACCEPT;

    while(operand_typearg != NULL)
    {
      if(!is_matchtype_operand_typearg(operand_typearg, pattern_typearg))
      {
        ok = MATCHTYPE_REJECT;
        break;
      }

      operand_typearg = ast_sibling(operand_typearg);
      pattern_typearg = ast_sibling(pattern_typearg);
    }
  }

  ast_free_unattached(r_type);
  return ok;
}

static matchtype_t is_matchtype_trait_trait(ast_t* operand, ast_t* pattern)
{
  // Operand cap/eph must be a subtype of pattern cap/eph for us to accept.
  // Otherwise deny.
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);
  ast_t* r_type = set_cap_and_ephemeral(operand, ast_id(p_cap), ast_id(p_eph));
  bool ok = is_subtype(operand, r_type);
  ast_free_unattached(r_type);

  return ok ? MATCHTYPE_ACCEPT : MATCHTYPE_DENY;
}

static matchtype_t is_matchtype_trait_nominal(ast_t* operand, ast_t* pattern)
{
  ast_t* def = (ast_t*)ast_data(pattern);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      return is_matchtype_trait_trait(operand, pattern);

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // Pattern must provide operand, ignoring cap/eph, for an accept.
      AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);
      ast_t* r_type = set_cap_and_ephemeral(operand,
        ast_id(p_cap), ast_id(p_eph));

      if(!is_subtype(pattern, r_type))
      {
        ast_free_unattached(r_type);
        return MATCHTYPE_REJECT;
      }

      // Operand cap/eph must be a subtype of pattern cap/eph for an accept.
      if(!is_subtype(operand, r_type))
      {
        ast_free_unattached(r_type);
        return MATCHTYPE_DENY;
      }

      return MATCHTYPE_ACCEPT;
    }

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_matchtype_operand_trait(ast_t* operand, ast_t* pattern)
{
  switch(ast_id(pattern))
  {
    case TK_NOMINAL:
      return is_matchtype_trait_nominal(operand, pattern);

    case TK_UNIONTYPE:
      return is_matchtype_pattern_union(operand, pattern);

    case TK_ISECTTYPE:
      return is_matchtype_pattern_isect(operand, pattern);

    case TK_TUPLETYPE:
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return is_matchtype_pattern_arrow(operand, pattern);

    case TK_TYPEPARAMREF:
      return is_matchtype_subtype_or_deny(operand, pattern);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_matchtype_operand_entity(ast_t* operand, ast_t* pattern)
{
  switch(ast_id(pattern))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return is_matchtype_subtype_or_deny(operand, pattern);

    case TK_UNIONTYPE:
      return is_matchtype_pattern_union(operand, pattern);

    case TK_ISECTTYPE:
      return is_matchtype_pattern_isect(operand, pattern);

    case TK_TUPLETYPE:
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return is_matchtype_pattern_arrow(operand, pattern);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_matchtype_operand_nominal(ast_t* operand, ast_t* pattern)
{
  ast_t* def = (ast_t*)ast_data(operand);

  switch(ast_id(def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return is_matchtype_operand_entity(operand, pattern);

    case TK_INTERFACE:
    case TK_TRAIT:
      return is_matchtype_operand_trait(operand, pattern);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_matchtype_operand_union(ast_t* operand, ast_t* pattern)
{
  // Some component of operand must be a possible match with the pattern type.
  matchtype_t r = MATCHTYPE_REJECT;

  for(ast_t* p = ast_child(operand); p != NULL; p = ast_sibling(p))
  {
    matchtype_t ok = is_matchtype(p, pattern);

    if(ok == MATCHTYPE_DENY)
      return MATCHTYPE_DENY;

    if(ok == MATCHTYPE_ACCEPT)
      r = MATCHTYPE_ACCEPT;
  }

  return r;
}

static matchtype_t is_matchtype_operand_isect(ast_t* operand, ast_t* pattern)
{
  // All components of operand must be a possible match with the pattern type.
  matchtype_t r = MATCHTYPE_ACCEPT;

  for(ast_t* p = ast_child(operand); p != NULL; p = ast_sibling(p))
  {
    matchtype_t ok = is_matchtype(p, pattern);

    if(ok == MATCHTYPE_DENY)
      return MATCHTYPE_DENY;

    if(ok == MATCHTYPE_REJECT)
      r = MATCHTYPE_REJECT;
  }

  return r;
}

static matchtype_t is_matchtype_tuple_tuple(ast_t* operand, ast_t* pattern)
{
  // Operand and pattern must pairwise match.
  ast_t* operand_child = ast_child(operand);
  ast_t* pattern_child = ast_child(pattern);

  while((operand_child != NULL) && (pattern_child != NULL))
  {
    matchtype_t ok = is_matchtype(operand_child, pattern_child);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    operand_child = ast_sibling(operand_child);
    pattern_child = ast_sibling(pattern_child);
  }

  if((operand_child == NULL) && (pattern_child == NULL))
    return MATCHTYPE_ACCEPT;

  return MATCHTYPE_REJECT;
}

static matchtype_t is_matchtype_operand_tuple(ast_t* operand, ast_t* pattern)
{
  switch(ast_id(pattern))
  {
    case TK_NOMINAL:
      return MATCHTYPE_REJECT;

    case TK_UNIONTYPE:
      return is_matchtype_pattern_union(operand, pattern);

    case TK_ISECTTYPE:
      return is_matchtype_pattern_isect(operand, pattern);

    case TK_TUPLETYPE:
      return is_matchtype_tuple_tuple(operand, pattern);

    case TK_ARROW:
      return is_matchtype_pattern_arrow(operand, pattern);

    case TK_TYPEPARAMREF:
      return MATCHTYPE_REJECT;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t is_matchtype_operand_arrow(ast_t* operand, ast_t* pattern)
{
  if(ast_id(pattern) == TK_ARROW)
  {
    // If we have the same viewpoint, check the right side.
    AST_GET_CHILDREN(operand, operand_left, operand_right);
    AST_GET_CHILDREN(pattern, pattern_left, pattern_right);
    bool check = false;

    switch(ast_id(operand_left))
    {
      case TK_THISTYPE:
        switch(ast_id(pattern_left))
        {
          case TK_THISTYPE:
          case TK_BOXTYPE:
            check = true;
            break;

          default: {}
        }
        break;

      case TK_BOXTYPE:
        check = ast_id(pattern_left) == TK_BOXTYPE;
        break;

      default:
        check = is_eqtype(operand_left, pattern_left);
        break;
    }

    if(check)
      return is_matchtype(operand_right, pattern_right);
  }

  // Check the lower bounds.
  ast_t* lower = viewpoint_lower(operand);
  matchtype_t ok = is_matchtype(pattern, lower);

  if(lower != operand)
    ast_free_unattached(lower);

  return ok;
}

static matchtype_t is_matchtype_operand_typeparam(ast_t* operand,
  ast_t* pattern)
{
  switch(ast_id(pattern))
  {
    case TK_NOMINAL:
    {
      // If operand's constraint could be a subtype of pattern, some
      // reifications could be a subtype of pattern.
      ast_t* operand_def = (ast_t*)ast_data(operand);
      ast_t* constraint = ast_childidx(operand_def, 1);

      if(ast_id(constraint) == TK_TYPEPARAMREF)
      {
        ast_t* constraint_def = (ast_t*)ast_data(constraint);

        if(constraint_def == operand_def)
          return MATCHTYPE_REJECT;
      }

      return is_matchtype(constraint, pattern);
    }

    case TK_UNIONTYPE:
      return is_matchtype_pattern_union(operand, pattern);

    case TK_ISECTTYPE:
      return is_matchtype_pattern_isect(operand, pattern);

    case TK_TUPLETYPE:
      // A type parameter can't be constrained to a tuple.
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return is_matchtype_pattern_arrow(operand, pattern);

    case TK_TYPEPARAMREF:
      // If the pattern is a typeparam, operand has to be a subtype.
      return is_matchtype_subtype_or_deny(operand, pattern);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

matchtype_t is_matchtype(ast_t* operand_type, ast_t* pattern_type)
{
  if(ast_id(pattern_type) == TK_DONTCARE)
    return MATCHTYPE_ACCEPT;

  switch(ast_id(operand_type))
  {
    case TK_NOMINAL:
      return is_matchtype_operand_nominal(operand_type, pattern_type);

    case TK_UNIONTYPE:
      return is_matchtype_operand_union(operand_type, pattern_type);

    case TK_ISECTTYPE:
      return is_matchtype_operand_isect(operand_type, pattern_type);

    case TK_TUPLETYPE:
      return is_matchtype_operand_tuple(operand_type, pattern_type);

    case TK_ARROW:
      return is_matchtype_operand_arrow(operand_type, pattern_type);

    case TK_TYPEPARAMREF:
      return is_matchtype_operand_typeparam(operand_type, pattern_type);

    case TK_FUNTYPE:
      return MATCHTYPE_DENY;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

