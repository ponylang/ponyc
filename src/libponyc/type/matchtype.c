#include "matchtype.h"
#include "subtype.h"
#include "viewpoint.h"
#include "cap.h"
#include "alias.h"
#include "reify.h"
#include "assemble.h"
#include <assert.h>

static matchtype_t could_subtype_with_union(ast_t* sub, ast_t* super)
{
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    matchtype_t ok = could_subtype(sub, child);

    if(ok == MATCHTYPE_ACCEPT)
      return MATCHTYPE_ACCEPT;

    child = ast_sibling(child);
  }

  return MATCHTYPE_REJECT;
}

static matchtype_t could_subtype_with_isect(ast_t* sub, ast_t* super)
{
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    matchtype_t ok = could_subtype(sub, child);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    child = ast_sibling(child);
  }

  return MATCHTYPE_ACCEPT;
}

static matchtype_t could_subtype_with_arrow(ast_t* sub, ast_t* super)
{
  // Check the lower bounds of super.
  ast_t* lower = viewpoint_lower(super);
  matchtype_t ok = could_subtype(sub, lower);

  if(lower != sub)
    ast_free_unattached(lower);

  return ok;
}

static matchtype_t could_subtype_or_deny(ast_t* sub, ast_t* super)
{
  // At this point, sub must be a subtype of super.
  if(is_subtype(sub, super))
    return MATCHTYPE_ACCEPT;

  // If sub would be a subtype of super if it had super's capability and
  // ephemerality, we deny other matches.
  token_id cap, eph;

  switch(ast_id(super))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
      cap = ast_id(sup_cap);
      eph = ast_id(sup_eph);
      break;
    }

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(super, sup_id, sup_cap, sup_eph);
      cap = ast_id(sup_cap);
      eph = ast_id(sup_eph);
      break;
    }

    default:
      assert(0);
      return MATCHTYPE_DENY;
  }

  ast_t* r_type = set_cap_and_ephemeral(sub, cap, eph);
  matchtype_t ok = MATCHTYPE_REJECT;

  if(is_subtype(r_type, super))
    ok = MATCHTYPE_DENY;

  ast_free_unattached(r_type);
  return ok;
}

static matchtype_t could_subtype_trait_trait(ast_t* sub, ast_t* super)
{
  // Sub cap/eph must be a subtype of super cap/eph, else deny.
  // Otherwise, accept.
  AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
  ast_t* r_type = set_cap_and_ephemeral(sub, ast_id(sup_cap), ast_id(sup_eph));
  bool ok = is_subtype(sub, r_type);
  ast_free_unattached(r_type);

  return ok ? MATCHTYPE_ACCEPT : MATCHTYPE_DENY;
}

static matchtype_t could_subtype_trait_nominal(ast_t* sub, ast_t* super)
{
  ast_t* def = (ast_t*)ast_data(super);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      return could_subtype_trait_trait(sub, super);

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    {
      // Super must provide sub, ignoring cap/eph, else reject.
      AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
      ast_t* r_type = set_cap_and_ephemeral(sub,
        ast_id(sup_cap), ast_id(sup_eph));

      if(!is_subtype(super, r_type))
      {
        ast_free_unattached(r_type);
        return MATCHTYPE_REJECT;
      }

      // Sub cap/eph must be a subtype of super cap/eph, else deny.
      if(!is_subtype(sub, r_type))
      {
        ast_free_unattached(r_type);
        return MATCHTYPE_DENY;
      }

      // Otherwise, accept.
      return MATCHTYPE_ACCEPT;
    }

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_trait(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return could_subtype_trait_nominal(sub, super);

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_or_deny(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_entity(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return could_subtype_or_deny(sub, super);

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_nominal(ast_t* sub, ast_t* super)
{
  ast_t* def = (ast_t*)ast_data(sub);

  switch(ast_id(def))
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return could_subtype_entity(sub, super);

    case TK_INTERFACE:
    case TK_TRAIT:
      return could_subtype_trait(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_union(ast_t* sub, ast_t* super)
{
  // Some component type must be a possible match with the supertype.
  ast_t* child = ast_child(sub);
  matchtype_t ok = MATCHTYPE_REJECT;

  while(child != NULL)
  {
    matchtype_t sub_ok = could_subtype(child, super);

    if(sub_ok != MATCHTYPE_REJECT)
      ok = sub_ok;

    if(ok == MATCHTYPE_DENY)
      return ok;

    child = ast_sibling(child);
  }

  return ok;
}

static matchtype_t could_subtype_isect(ast_t* sub, ast_t* super)
{
  // If any component is a match, we're a match. Otherwise return the worst
  // of reject or deny.
  ast_t* child = ast_child(sub);
  matchtype_t ok = MATCHTYPE_REJECT;

  while(child != NULL)
  {
    matchtype_t sub_ok = could_subtype(child, super);

    if(sub_ok == MATCHTYPE_ACCEPT)
      return sub_ok;

    if(ok == MATCHTYPE_DENY)
      ok = sub_ok;

    child = ast_sibling(child);
  }

  return ok;
}

static matchtype_t could_subtype_tuple_tuple(ast_t* sub, ast_t* super)
{
  // Must pairwise match with the supertype.
  ast_t* sub_child = ast_child(sub);
  ast_t* super_child = ast_child(super);

  while((sub_child != NULL) && (super_child != NULL))
  {
    matchtype_t ok = could_subtype(sub_child, super_child);

    if(ok != MATCHTYPE_ACCEPT)
      return ok;

    sub_child = ast_sibling(sub_child);
    super_child = ast_sibling(super_child);
  }

  if((sub_child == NULL) && (super_child == NULL))
    return MATCHTYPE_ACCEPT;

  return MATCHTYPE_REJECT;
}

static matchtype_t could_subtype_tuple(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return MATCHTYPE_REJECT;

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      return could_subtype_tuple_tuple(sub, super);

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return MATCHTYPE_REJECT;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

static matchtype_t could_subtype_arrow(ast_t* sub, ast_t* super)
{
  if(ast_id(super) == TK_ARROW)
  {
    // If we have the same viewpoint, check the right side.
    AST_GET_CHILDREN(sub, sub_left, sub_right);
    AST_GET_CHILDREN(super, super_left, super_right);
    bool check = false;

    switch(ast_id(sub_left))
    {
      case TK_THISTYPE:
        switch(ast_id(super_left))
        {
          case TK_THISTYPE:
          case TK_BOXTYPE:
            check = true;
            break;

          default: {}
        }
        break;

      case TK_BOXTYPE:
        check = ast_id(super_left) == TK_BOXTYPE;
        break;

      default:
        check = is_eqtype(sub_left, super_left);
        break;
    }

    if(check)
      return could_subtype(sub_right, super_right);
  }

  // Check the lower bounds.
  ast_t* lower = viewpoint_lower(sub);
  matchtype_t ok = could_subtype(super, lower);

  if(lower != sub)
    ast_free_unattached(lower);

  return ok;
}

static matchtype_t could_subtype_typeparam(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
    {
      // If our constraint could be a subtype of super, some reifications could
      // be a subtype of super.
      ast_t* sub_def = (ast_t*)ast_data(sub);
      ast_t* constraint = ast_childidx(sub_def, 1);

      if(ast_id(constraint) == TK_TYPEPARAMREF)
      {
        ast_t* constraint_def = (ast_t*)ast_data(constraint);

        if(constraint_def == sub_def)
          return MATCHTYPE_REJECT;
      }

      return could_subtype(constraint, super);
    }

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      // A type parameter can't be constrained to a tuple.
      return MATCHTYPE_REJECT;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      // If the supertype is a typeparam, we have to be a subtype.
      return could_subtype_or_deny(sub, super);

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

matchtype_t could_subtype(ast_t* sub, ast_t* super)
{
  if(ast_id(super) == TK_DONTCARE)
    return MATCHTYPE_ACCEPT;

  // Does a subtype of sub exist that is a subtype of super?
  switch(ast_id(sub))
  {
    case TK_NOMINAL:
      return could_subtype_nominal(sub, super);

    case TK_UNIONTYPE:
      return could_subtype_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_isect(sub, super);

    case TK_TUPLETYPE:
      return could_subtype_tuple(sub, super);

    case TK_ARROW:
      return could_subtype_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_typeparam(sub, super);

    case TK_FUNTYPE:
      return MATCHTYPE_DENY;

    default: {}
  }

  assert(0);
  return MATCHTYPE_DENY;
}

