#include "cap.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include <assert.h>

static token_id cap_union_constraint(token_id a, token_id b)
{
  // Decide a capability for a type parameter that is constrained by a union.
  // We don't get box or tag here, but we do get boxgen, taggen and anygen.
  if(a == b)
    return a;

  // Pick a type that allows only what both types allow.
  switch(a)
  {
    case TK_REF:
      switch(b)
      {
        case TK_VAL:
        case TK_BOX_GENERIC:
          return TK_BOX_GENERIC;

        default: {}
      }
      break;

    case TK_VAL:
      switch(b)
      {
        case TK_REF:
        case TK_BOX_GENERIC:
          return TK_BOX_GENERIC;

        case TK_TAG_GENERIC:
          return TK_TAG_GENERIC;

        default: {}
      }

    case TK_BOX_GENERIC:
      switch(b)
      {
        case TK_REF:
        case TK_VAL:
          return TK_BOX_GENERIC;

        default: {}
      }
      break;

    case TK_TAG_GENERIC:
      switch(b)
      {
        case TK_VAL:
          return TK_TAG_GENERIC;

        default: {}
      }
      break;

    default: {}
  }

  return TK_ANY_GENERIC;
}

static token_id cap_isect_constraint(token_id a, token_id b)
{
  // Decide a capability for a type parameter that is constrained by an isect.
  // We don't get box or tag here, but we do get boxgen, taggen and anygen.
  if(is_cap_sub_cap(a, b))
    return a;

  if(is_cap_sub_cap(b, a))
    return b;

  return TK_ANY_GENERIC;
}

bool is_cap_sub_cap(token_id sub, token_id super)
{
  switch(sub)
  {
    case TK_ISO:
      return true;

    case TK_TRN:
      switch(super)
      {
        case TK_TRN:
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_BOX_GENERIC:
        case TK_TAG_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(super)
      {
        case TK_REF:
        case TK_BOX:
        case TK_TAG:
        case TK_BOX_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(super)
      {
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_BOX_GENERIC:
        case TK_TAG_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_BOX:
      switch(super)
      {
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_TAG:
      return super == TK_TAG;

    case TK_BOX_GENERIC:
      switch(super)
      {
        case TK_BOX:
        case TK_TAG:
        case TK_BOX_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_TAG_GENERIC:
      switch(super)
      {
        case TK_TAG:
        case TK_TAG_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_ANY_GENERIC:
      switch(super)
      {
        case TK_TAG:
        case TK_ANY_GENERIC:
          return true;

        default: {}
      }
      break;

    default: {}
  }

  return false;
}

token_id cap_single(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      return ast_id(ast_childidx(type, 3));

    case TK_TYPEPARAMREF:
      return ast_id(ast_childidx(type, 1));

    default: {}
  }

  assert(0);
  return TK_NONE;
}

token_id cap_for_this(typecheck_t* t)
{
  // If this is a primitive, it's a val.
  if(ast_id(t->frame->type) == TK_PRIMITIVE)
    return TK_VAL;

  // If we aren't in a method, we're in a field initialiser.
  if(t->frame->method == NULL)
    return TK_REF;

  // If it's a function, get the capability from the signature.
  if(ast_id(t->frame->method) == TK_FUN)
    return ast_id(ast_child(t->frame->method));

  // If it's a behaviour or a constructor, it's a ref.
  return TK_REF;
}

token_id cap_viewpoint(token_id view, token_id cap)
{
  switch(view)
  {
    case TK_ISO:
    {
      switch(cap)
      {
        case TK_ISO: return TK_ISO;
        case TK_VAL: return TK_VAL;
        case TK_TAG_GENERIC: return TK_TAG_GENERIC;
        default: return TK_TAG;
      }
      break;
    }

    case TK_TRN:
    {
      switch(cap)
      {
        case TK_ISO: return TK_ISO;
        case TK_TRN: return TK_TRN;
        case TK_VAL: return TK_VAL;
        case TK_TAG: return TK_TAG;
        case TK_TAG_GENERIC: return TK_TAG_GENERIC;
        case TK_ANY_GENERIC: return TK_TAG;
        default: return TK_BOX;
      }
    }

    case TK_REF:
      return cap;

    case TK_VAL:
    {
      switch(cap)
      {
        case TK_TAG: return TK_TAG;
        case TK_TAG_GENERIC: return TK_TAG_GENERIC;
        case TK_ANY_GENERIC: return TK_TAG;
        default: return TK_VAL;
      }
      break;
    }

    case TK_BOX:
    {
      switch(cap)
      {
        case TK_ISO: return TK_TAG;
        case TK_VAL: return TK_VAL;
        case TK_TAG: return TK_TAG;
        case TK_TAG_GENERIC: return TK_TAG_GENERIC;
        case TK_ANY_GENERIC: return TK_TAG;
        default: return TK_BOX;
      }
      break;
    }

    default: {}
  }

  assert(0);
  return TK_NONE;
}

/**
 * Given the capability of a constraint, derive the capability to use for a
 * typeparamref of that constraint.
 */
static token_id cap_typeparam(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_ISO;
    case TK_TRN: return TK_TRN;
    case TK_REF: return TK_REF;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_BOX_GENERIC;
    case TK_TAG: return TK_TAG_GENERIC;
    case TK_BOX_GENERIC: return TK_BOX_GENERIC;
    case TK_TAG_GENERIC: return TK_TAG_GENERIC;
    case TK_ANY_GENERIC: return TK_ANY_GENERIC;
    case TK_NONE: return TK_ANY_GENERIC;
    default: {}
  }

  assert(0);
  return TK_NONE;
}

bool cap_sendable(token_id cap, token_id eph)
{
  switch(cap)
  {
    case TK_ISO:
      return eph != TK_BORROWED;

    case TK_VAL:
    case TK_TAG:
    case TK_TAG_GENERIC:
      return true;

    default: {}
  }

  return false;
}

bool cap_safetowrite(token_id into, token_id cap)
{
  switch(into)
  {
    case TK_ISO:
      switch(cap)
      {
        case TK_ISO:
        case TK_VAL:
        case TK_TAG:
        case TK_TAG_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_TRN:
      switch(cap)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_VAL:
        case TK_TAG:
        case TK_TAG_GENERIC:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      return true;

    default: {}
  }

  return false;
}

token_id cap_from_constraint(ast_t* type)
{
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
    case TK_TYPEPARAMREF:
      return cap_typeparam(cap_single(type));

    default: {}
  }

  assert(0);
  return TK_NONE;
}
