#include "cap.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include <assert.h>

static token_id cap_upper_bounds(token_id a, token_id b)
{
  assert((a >= TK_ISO) && (a <= TK_TAG));
  assert((b >= TK_ISO) && (b <= TK_TAG));

  if(is_cap_sub_cap(a, b))
    return b;

  if(is_cap_sub_cap(b, a))
    return a;

  // one is ref and the other is val
  return TK_BOX;
}

static token_id cap_lower_bounds(token_id a, token_id b)
{
  assert((a >= TK_ISO) && (a <= TK_TAG));
  assert((b >= TK_ISO) && (b <= TK_TAG));

  if(is_cap_sub_cap(a, b))
    return a;

  if(is_cap_sub_cap(b, a))
    return b;

  // one is ref and the other is val
  return TK_TRN;
}

bool is_cap_sub_cap(token_id sub, token_id super)
{
  assert((sub >= TK_ISO) && (sub <= TK_TAG));
  assert((super >= TK_ISO) && (super <= TK_TAG));

  if((sub == TK_REF) && (super == TK_VAL))
    return false;

  return sub <= super;
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
 * typeparamref of that constraint. Returns the lowest capability that could
 * be a subtype of the constraint.
 */
static token_id cap_typeparam(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_ISO;
    case TK_TRN: return TK_TRN;
    case TK_REF: return TK_REF;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_BOX;
    case TK_TAG: return TK_ISO;
    case TK_NONE: return TK_ISO; // For typeparamref in constraints.
    default: {}
  }

  assert(0);
  return TK_NONE;
}

bool cap_sendable(token_id cap)
{
  switch(cap)
  {
    case TK_ISO:
    case TK_VAL:
    case TK_TAG:
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
      token_id cap = TK_ISO;

      while(child != NULL)
      {
        cap = cap_upper_bounds(cap, cap_from_constraint(child));
        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);
      token_id cap = TK_TAG;

      while(child != NULL)
      {
        cap = cap_lower_bounds(cap, cap_from_constraint(child));
        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return cap_typeparam(cap_single(type));

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      return cap_typeparam(cap_viewpoint(TK_BOX, cap_single(right)));
    }

    default: {}
  }

  assert(0);
  return TK_NONE;
}
