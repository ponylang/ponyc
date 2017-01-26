#include "typeparam.h"
#include "cap.h"
#include "subtype.h"
#include "../ast/token.h"
#include <assert.h>

static token_id cap_union_constraint(token_id a, token_id b)
{
  // Decide a capability for a type parameter that is constrained by a union.
  if(a == b)
    return a;

  // If we're in a set together, return the set. Otherwise, use #any.
  switch(a)
  {
    case TK_ISO:
      switch(b)
      {
        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
          return TK_CAP_SEND;

        default: {}
      }
      break;

    case TK_REF:
      switch(b)
      {
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:
          return TK_CAP_READ;

        default: {}
      }
      break;

    case TK_VAL:
      switch(b)
      {
        case TK_REF:
        case TK_BOX:
        case TK_CAP_READ:
          return TK_CAP_READ;

        case TK_TAG:
        case TK_CAP_SHARE:
          return TK_CAP_SHARE;

        case TK_ISO:
        case TK_CAP_SEND:
          return TK_CAP_SEND;

        default: {}
      }

    case TK_BOX:
      switch(b)
      {
        case TK_REF:
        case TK_VAL:
        case TK_CAP_READ:
          return TK_CAP_READ;

        default: {}
      }
      break;

    case TK_TAG:
      switch(b)
      {
        case TK_VAL:
        case TK_CAP_SHARE:
          return TK_CAP_SHARE;

        case TK_ISO:
        case TK_CAP_SEND:
          return TK_CAP_SEND;

        default: {}
      }
      break;

    case TK_CAP_READ:
      switch(b)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
          return TK_CAP_READ;

        default: {}
      }
      break;

    case TK_CAP_SEND:
      switch(b)
      {
        case TK_ISO:
        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SHARE:
          return TK_CAP_SEND;

        default: {}
      }
      break;

    case TK_CAP_SHARE:
      switch(b)
      {
        case TK_VAL:
        case TK_TAG:
          return TK_CAP_SHARE;

        case TK_ISO:
        case TK_CAP_SEND:
          return TK_CAP_SEND;

        default: {}
      }
      break;

    case TK_CAP_ALIAS:
      switch(b)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_CAP_READ:
          return TK_CAP_ALIAS;

        default: {}
      }
      break;

    default: {}
  }

  return TK_CAP_ANY;
}

static token_id cap_isect_constraint(token_id a, token_id b)
{
  // Decide a capability for a type parameter that is constrained by an isect.
  if(a == b)
    return a;

  // If one side is #any, always use the other side.
  if(a == TK_CAP_ANY)
    return b;

  if(b == TK_CAP_ANY)
    return a;

  // If we're in a set, extract us from the set. Otherwise, use #any.
  switch(a)
  {
    case TK_ISO:
      switch(b)
      {
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
          return TK_ISO;

        default: {}
      }
      break;

    case TK_REF:
      switch(b)
      {
        case TK_CAP_READ:
        case TK_CAP_ALIAS:
          return TK_REF;

        default: {}
      }
      break;

    case TK_VAL:
      switch(b)
      {
        case TK_CAP_READ:
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
        case TK_CAP_ALIAS:
          return TK_VAL;

        default: {}
      }
      break;

    case TK_BOX:
      switch(b)
      {
        case TK_CAP_READ:
        case TK_CAP_ALIAS:
          return TK_BOX;

        default: {}
      }
      break;

    case TK_CAP_READ:
      switch(b)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
          return b;

        case TK_CAP_ALIAS:
          return TK_CAP_READ;

        case TK_CAP_SEND:
        case TK_CAP_SHARE:
          return TK_VAL;

        default: {}
      }
      break;

    case TK_CAP_SEND:
      switch(b)
      {
        case TK_ISO:
        case TK_VAL:
        case TK_TAG:
          return b;

        case TK_CAP_READ:
          return TK_VAL;

        case TK_CAP_SHARE:
        case TK_CAP_ALIAS:
          return TK_CAP_SHARE;

        default: {}
      }
      break;

    case TK_CAP_SHARE:
      switch(b)
      {
        case TK_VAL:
        case TK_TAG:
          return b;

        case TK_CAP_READ:
          return TK_VAL;

        case TK_CAP_SEND:
        case TK_CAP_ALIAS:
          return TK_CAP_SHARE;

        default: {}
      }

    case TK_CAP_ALIAS:
      switch(b)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
          return b;

        case TK_CAP_READ:
          return TK_CAP_READ;

        case TK_CAP_SEND:
        case TK_CAP_SHARE:
          return TK_CAP_SHARE;

        default: {}
      }

    default: {}
  }

  return TK_CAP_ANY;
}

static token_id cap_from_constraint(ast_t* type)
{
  if(type == NULL)
    return TK_CAP_ANY;

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(type);
      token_id cap = cap_from_constraint(child);
      child = ast_sibling(child);

      while(child != NULL)
      {
        cap = cap_union_constraint(cap, cap_from_constraint(child));
        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);
      token_id cap = cap_from_constraint(child);
      child = ast_sibling(child);

      while(child != NULL)
      {
        cap = cap_isect_constraint(cap, cap_from_constraint(child));
        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_NOMINAL:
      return cap_single(type);

    case TK_TYPEPARAMREF:
      return cap_from_constraint(typeparam_constraint(type));

    default: {}
  }

  // Tuples and arrows can't be used as constraints.
  assert(0);
  return TK_NONE;
}

static bool apply_cap_to_single(ast_t* type, token_id tcap, token_id teph)
{
  ast_t* cap = cap_fetch(type);
  ast_t* eph = ast_sibling(cap);

  // Return false if the tcap is not a possible reification of the cap.
  switch(ast_id(cap))
  {
    case TK_CAP_SEND:
      switch(tcap)
      {
        case TK_ISO:
        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
          break;

        default:
          return false;
      }
      break;

    case TK_CAP_SHARE:
      switch(tcap)
      {
        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SHARE:
          break;

        default:
          return false;
      }
      break;

    case TK_CAP_READ:
      switch(tcap)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:
          break;

        default:
          return false;
      }
      break;

    case TK_CAP_ALIAS:
      switch(tcap)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_CAP_SHARE:
        case TK_CAP_READ:
        case TK_CAP_ALIAS:
          break;

        default:
          return false;
      }
      break;

    case TK_CAP_ANY:
      break;

    default:
      if(ast_id(cap) != tcap)
        return false;
  }

  // Set the capability.
  ast_setid(cap, tcap);

  // Apply the ephemerality.
  switch(ast_id(eph))
  {
    case TK_EPHEMERAL:
      if(teph == TK_ALIASED)
        ast_setid(eph, TK_NONE);
      break;

    case TK_NONE:
      ast_setid(eph, teph);
      break;

    case TK_ALIASED:
      if(teph == TK_EPHEMERAL)
        ast_setid(eph, TK_NONE);
      break;

    default:
      assert(0);
      return false;
  }

  return true;
}

static bool apply_cap(ast_t* type, token_id tcap, token_id teph)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* next = ast_sibling(child);

        if(!apply_cap(child, tcap, teph))
          ast_remove(child);

        child = next;
      }

      return ast_childcount(type) > 0;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return apply_cap_to_single(type, tcap, teph);

    default: {}
  }

  assert(0);
  return false;
}

static ast_t* constraint_cap(ast_t* typeparamref)
{
  ast_t* constraint = typeparam_constraint(typeparamref);

  if(constraint == NULL)
    return NULL;

  AST_GET_CHILDREN(typeparamref, id, cap, eph);
  token_id tcap = ast_id(cap);
  token_id teph = ast_id(eph);

  ast_t* r_constraint = ast_dup(constraint);

  if(!apply_cap(r_constraint, tcap, teph))
  {
    ast_free_unattached(r_constraint);
    return NULL;
  }

  return r_constraint;
}

ast_t* typeparam_constraint(ast_t* typeparamref)
{
  assert(ast_id(typeparamref) == TK_TYPEPARAMREF);
  ast_t* def = (ast_t*)ast_data(typeparamref);
  ast_t* constraint = ast_childidx(def, 1);
  astlist_t* def_list = astlist_push(NULL, def);

  while(ast_id(constraint) == TK_TYPEPARAMREF)
  {
    ast_t* constraint_def = (ast_t*)ast_data(constraint);

    if(astlist_find(def_list, constraint_def))
    {
      constraint = NULL;
      break;
    }

    def_list = astlist_push(def_list, constraint_def);
    constraint = ast_childidx(constraint_def, 1);
  }

  astlist_free(def_list);
  return constraint;
}

ast_t* typeparam_upper(ast_t* typeparamref)
{
  assert(ast_id(typeparamref) == TK_TYPEPARAMREF);
  return constraint_cap(typeparamref);
}

ast_t* typeparam_lower(ast_t* typeparamref)
{
  assert(ast_id(typeparamref) == TK_TYPEPARAMREF);
  ast_t* constraint = typeparam_constraint(typeparamref);

  // A constraint must be a final type (primitive, struct, class, actor) in
  // order to have a lower bound.
  if(!is_known(constraint))
    return NULL;

  return constraint_cap(typeparamref);
}

void typeparam_set_cap(ast_t* typeparamref)
{
  assert(ast_id(typeparamref) == TK_TYPEPARAMREF);
  AST_GET_CHILDREN(typeparamref, id, cap, eph);
  ast_t* constraint = typeparam_constraint(typeparamref);
  token_id tcap = cap_from_constraint(constraint);
  ast_setid(cap, tcap);
}
