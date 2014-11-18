#include "matchtype.h"
#include "subtype.h"
#include "viewpoint.h"
#include "cap.h"
#include "alias.h"
#include "reify.h"
#include "assemble.h"
#include <assert.h>

static bool could_subtype_with_union(ast_t* sub, ast_t* super)
{
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    child = ast_sibling(child);

    if(could_subtype(sub, child))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

static bool could_subtype_with_isect(ast_t* sub, ast_t* super)
{
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    child = ast_sibling(child);

    if(!could_subtype(sub, child))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static bool could_subtype_with_arrow(ast_t* sub, ast_t* super)
{
  // Check the lower bounds of super.
  ast_t* lower = viewpoint_lower(super);
  bool ok = could_subtype(sub, lower);

  if(lower != sub)
    ast_free_unattached(lower);

  return ok;
}

static bool could_subtype_with_typeparam(ast_t* sub, ast_t* super)
{
  // Could only be a subtype if it is a subtype, since we don't know the lower
  // bounds of the constraint any more accurately than is_subtype() does.
  return is_subtype(sub, super);
}

static bool could_subtype_trait_trait(ast_t* sub, ast_t* super)
{
  // The subtype cap/eph must be a subtype of the supertype cap/eph.
  AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
  ast_t* r_type = set_cap_and_ephemeral(sub, ast_id(sup_cap), ast_id(sup_eph));
  bool ok = is_subtype(sub, r_type);
  ast_free_unattached(r_type);

  return ok;
}

static bool could_subtype_trait_nominal(ast_t* sub, ast_t* super)
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
      // If we set sub cap/eph to the same as super, both sub and super must
      // be a subtype of the new type. That is, the concrete type must provide
      // the trait and the trait must be a subtype of the concrete type's
      // capability and ephemerality.
      AST_GET_CHILDREN(super, sup_pkg, sup_id, sup_typeargs, sup_cap, sup_eph);
      ast_t* r_type = set_cap_and_ephemeral(sub,
        ast_id(sup_cap), ast_id(sup_eph));

      bool ok = is_subtype(sub, r_type) && is_subtype(super, r_type);
      ast_free_unattached(r_type);
      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool could_subtype_trait(ast_t* sub, ast_t* super)
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
      return false;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_with_typeparam(sub, super);

    default: {}
  }

  assert(0);
  return false;
}

static bool could_subtype_nominal(ast_t* sub, ast_t* super)
{
  ast_t* def = (ast_t*)ast_data(sub);

  switch(ast_id(def))
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      // With a concrete type, the subtype must be a subtype of the supertype.
      return is_subtype(sub, super);

    case TK_INTERFACE:
    case TK_TRAIT:
      return could_subtype_trait(sub, super);

    default: {}
  }

  assert(0);
  return false;
}

static bool could_subtype_union(ast_t* sub, ast_t* super)
{
  // Some component type must be a possible match with the supertype.
  ast_t* child = ast_child(sub);

  while(child != NULL)
  {
    if(could_subtype(child, super))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

static bool could_subtype_isect(ast_t* sub, ast_t* super)
{
  // All components type must be a possible match with the supertype.
  ast_t* child = ast_child(sub);

  while(child != NULL)
  {
    if(!could_subtype(child, super))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static bool could_subtype_tuple_tuple(ast_t* sub, ast_t* super)
{
  // Must pairwise match with the supertype.
  ast_t* sub_child = ast_child(sub);
  ast_t* super_child = ast_child(super);

  while((sub_child != NULL) && (super_child != NULL))
  {
    if(!could_subtype(sub_child, super_child))
      return false;

    sub_child = ast_sibling(sub_child);
    super_child = ast_sibling(super_child);
  }

  return (sub_child == NULL) && (super_child == NULL);
}

static bool could_subtype_tuple(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return is_subtype(sub, super);

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      return could_subtype_tuple_tuple(sub, super);

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      return could_subtype_with_typeparam(sub, super);

    default: {}
  }

  assert(0);
  return false;
}

static bool could_subtype_arrow(ast_t* sub, ast_t* super)
{
  if(ast_id(super) == TK_ARROW)
  {
    // If we have the same viewpoint, check the right side.
    AST_GET_CHILDREN(sub, sub_left, sub_right);
    AST_GET_CHILDREN(super, super_left, super_right);

    if(is_eqtype(sub_left, super_left) && could_subtype(sub_right, super_right))
      return true;
  }

  // Check the upper bounds.
  ast_t* upper = viewpoint_upper(sub);
  bool ok = could_subtype(upper, super);

  if(upper != sub)
    ast_free_unattached(upper);

  return ok;
}

static bool could_subtype_typeparam(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
    {
      // If we are a subtype, then we also could be.
      if(is_subtype(sub, super))
        return true;

      // If our constraint could be a subtype of super, all reifications could
      // be a subtype of super.
      ast_t* sub_def = (ast_t*)ast_data(sub);
      ast_t* constraint = ast_childidx(sub_def, 1);

      if(ast_id(constraint) == TK_TYPEPARAMREF)
      {
        ast_t* constraint_def = (ast_t*)ast_data(constraint);

        if(constraint_def == sub_def)
          return false;
      }

      return could_subtype(constraint, super);
    }

    case TK_UNIONTYPE:
      return could_subtype_with_union(sub, super);

    case TK_ISECTTYPE:
      return could_subtype_with_isect(sub, super);

    case TK_TUPLETYPE:
      // A type parameter can't be constrainted to a tuple.
      return false;

    case TK_ARROW:
      return could_subtype_with_arrow(sub, super);

    case TK_TYPEPARAMREF:
      // If the supertype is a typeparam, we have to be a subtype.
      return is_subtype(sub, super);

    default: {}
  }

  assert(0);
  return false;
}

bool could_subtype(ast_t* sub, ast_t* super)
{
  // If we are a subtype, then we also could be.
  if(is_subtype(sub, super))
    return true;

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

    default: {}
  }

  assert(0);
  return false;
}
