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

token_id cap_for_receiver(ast_t* ast)
{
  // If this is a primitive, it's a val.
  ast_t* def = ast_enclosing_type(ast);

  if(ast_id(def) == TK_PRIMITIVE)
    return TK_VAL;

  ast_t* method = ast_enclosing_method(ast);

  // If we aren't in a method, we're in a field initialiser.
  if(method == NULL)
    return TK_REF;

  // If it's a function, get the capability from the signature.
  if(ast_id(method) == TK_FUN)
    return ast_id(ast_child(method));

  // If it's a behaviour or a constructor, it's a ref.
  return TK_REF;
}

token_id cap_for_type(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(type);
      token_id cap = TK_ISO;

      while(child != NULL)
      {
        cap = cap_upper_bounds(cap, cap_for_type(child));
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
        cap = cap_lower_bounds(cap, cap_for_type(child));
        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_TUPLETYPE:
    {
      // should never be looking for the cap of a tuple
      break;
    }

    case TK_NOMINAL:
      return ast_id(ast_childidx(type, 3));

    case TK_TYPEPARAMREF:
      return ast_id(ast_childidx(type, 1));

    case TK_ARROW:
    {
      // arrow types arise when something could be box, ref or val. cap_for_type
      // is used for receiver cap, sendability and safe to write. for all of
      // these, we can treat any arrow as a view through box.
      return cap_viewpoint(TK_BOX, cap_for_type(ast_childidx(type, 1)));
    }

    default: {}
  }

  assert(0);
  return TK_NONE;
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

token_id cap_viewpoint_lower(token_id cap)
{
  // For any chain of arrows, return a capability that is a subtype of the
  // resulting capability.
  // iso <: ref->iso, iso <: val->iso, iso <: box->iso
  // trn <: ref->trn, trn <: val->trn, trn <: box->trn
  // val <: ref->val, val <: val->val, val <: box->val
  // val <: ref->box, val <: val->val, val <: box->box
  // tag <: ref->tag, tag <: val->tag, tag <: box->tag
  switch(cap)
  {
    case TK_ISO: return TK_ISO;
    case TK_TRN: return TK_TRN;
    case TK_REF: return TK_TRN;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_VAL;
    case TK_TAG: return TK_TAG;
    default: {}
  }

  assert(0);
  return TK_NONE;
}

token_id cap_alias(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_TAG;
    case TK_TRN: return TK_BOX;
    case TK_REF: return TK_REF;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_BOX;
    case TK_TAG: return TK_TAG;
    default: {}
  }

  assert(0);
  return TK_NONE;
}

token_id cap_alias_bind(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_TAG;
    case TK_TRN: return TK_TAG; // Prevent trn from binding to a box constraint.
    case TK_REF: return TK_REF;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_BOX;
    case TK_TAG: return TK_TAG;
    default: {}
  }

  assert(0);
  return TK_NONE;
}

token_id cap_recover(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_ISO;
    case TK_TRN: return TK_ISO;
    case TK_REF: return TK_ISO;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_VAL;
    case TK_TAG: return TK_TAG;
    default: {}
  }

  assert(0);
  return TK_NONE;
}

token_id cap_send(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_ISO;
    case TK_TRN: return TK_TAG;
    case TK_REF: return TK_TAG;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_TAG;
    case TK_TAG: return TK_TAG;
    default: {}
  }

  assert(0);
  return TK_NONE;
}

token_id cap_typeparam(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_ISO;
    case TK_TRN: return TK_TRN;
    case TK_REF: return TK_REF;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_BOX;
    case TK_TAG: return TK_ISO;
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

bool cap_compatible(ast_t* a, ast_t* b)
{
  token_id ta = cap_for_type(a);
  token_id tb = cap_for_type(b);

  switch(ta)
  {
    case TK_ISO:
      return tb == TK_TAG;

    case TK_TRN:
      switch(tb)
      {
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(tb)
      {
        case TK_REF:
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(tb)
      {
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_BOX:
      return tb != TK_ISO;

    case TK_TAG:
      return true;

    default: {}
  }

  return false;
}
