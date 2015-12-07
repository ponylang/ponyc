#include "cap.h"
#include "../ast/token.h"
#include "viewpoint.h"
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
          return TK_VAL;

        default: {}
      }
      break;

    case TK_BOX:
      switch(b)
      {
        case TK_CAP_READ:
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

        case TK_CAP_SHARE:
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

        case TK_CAP_SEND:
          return TK_CAP_SHARE;

        default: {}
      }

    default: {}
  }

  return TK_CAP_ANY;
}

// The resulting eph will always be TK_EPHEMERAL or TK_NONE.
static void cap_aliasing(token_id* cap, token_id* eph)
{
  switch(*eph)
  {
    case TK_EPHEMERAL:
      switch(*cap)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          break;

        default:
          // Everything else unaliases as itself, so use TK_NONE.
          *eph = TK_NONE;
      }
      break;

    case TK_BORROWED:
      switch(*cap)
      {
        case TK_ISO:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          // Alias as tag.
          *cap = TK_TAG;
          break;

        case TK_TRN:
          // Alias as box.
          *cap = TK_BOX;
          break;

        case TK_ISO_BIND:
          *cap = TK_TAG_BIND;
          break;

        case TK_TRN_BIND:
          *cap = TK_BOX_BIND;
          break;

        case TK_CAP_SEND_BIND:
          *cap = TK_CAP_SHARE_BIND;
          break;

        default: {}
      }

      *eph = TK_NONE;
      break;

    default: {}
  }
}

bool is_cap_sub_cap(token_id sub, token_id subalias, token_id super,
  token_id supalias)
{
  // Transform the cap based on the aliasing info.
  cap_aliasing(&sub, &subalias);
  cap_aliasing(&super, &supalias);

  if(supalias == TK_EPHEMERAL)
  {
    // Sub must be ephemeral.
    if(subalias != TK_EPHEMERAL)
      return false;
  }

  if((sub == super) || (super == TK_TAG) || (super == TK_CAP_ANY_BIND))
    return true;

  // Every possible value of sub must be a subtype of every possible value of
  // super.
  switch(sub)
  {
    case TK_ISO:
      switch(super)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:

        case TK_ISO_BIND:
        case TK_CAP_SEND_BIND:
          return true;

        default: {}
      }
      break;

    case TK_TRN:
      switch(super)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:
        case TK_CAP_SHARE:

        case TK_TRN_BIND:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(super)
      {
        case TK_BOX:

        case TK_REF_BIND:
        case TK_CAP_READ_BIND:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(super)
      {
        case TK_BOX:
        case TK_CAP_SHARE:

        case TK_VAL_BIND:
        case TK_CAP_READ_BIND:
        case TK_CAP_SEND_BIND:
        case TK_CAP_SHARE_BIND:
          return true;

        default: {}
      }
      break;

    case TK_BOX:
      switch(super)
      {
        case TK_BOX_BIND:
        case TK_CAP_READ_BIND:
          return true;

        default: {}
      }
      break;

    case TK_TAG:
      switch(super)
      {
        case TK_TAG_BIND:
        case TK_CAP_SEND_BIND:
        case TK_CAP_SHARE_BIND:
          return true;

        default: {}
      }
      break;

    case TK_CAP_READ:
      switch(super)
      {
        case TK_BOX:
        case TK_CAP_READ_BIND:
          return true;

        default: {}
      }
      break;

    case TK_CAP_SEND:
      switch(super)
      {
        case TK_CAP_SEND_BIND:
          return true;

        default: {}
      }
      break;

    case TK_CAP_SHARE:
      switch(super)
      {
        case TK_CAP_SHARE_BIND:
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
  size_t i;

  switch(ast_id(type))
  {
    case TK_NOMINAL:
      i = 3;
      break;

    case TK_TYPEPARAMREF:
      i = 1;
      break;

    default:
      assert(0);
      return TK_NONE;
  }

  ast_t* cap = ast_childidx(type, i);
  ast_t* eph = ast_sibling(cap);

  token_id tcap = ast_id(cap);
  token_id teph = ast_id(eph);
  cap_aliasing(&tcap, &teph);

  return tcap;
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
  // Can't see through these viewpoints.
  switch(view)
  {
    case TK_TAG:
    case TK_TAG_BIND:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ANY:
    case TK_CAP_SEND_BIND:
    case TK_CAP_SHARE_BIND:
    case TK_CAP_ANY_BIND:
      return TK_NONE;

    default: {}
  }

  // A tag is always seen as a tag.
  if(cap == TK_TAG)
    return TK_TAG;

  switch(view)
  {
    case TK_ISO:
    case TK_ISO_BIND:
    {
      switch(cap)
      {
        case TK_ISO: return TK_ISO;
        case TK_VAL: return TK_VAL;
        case TK_CAP_SEND: return TK_CAP_SEND;
        case TK_CAP_SHARE: return TK_CAP_SHARE;
        default: return TK_TAG;
      }
      break;
    }

    case TK_TRN:
    case TK_TRN_BIND:
    {
      switch(cap)
      {
        case TK_ISO: return TK_ISO;
        case TK_TRN: return TK_TRN;
        case TK_VAL: return TK_VAL;
        case TK_CAP_SEND: return TK_CAP_SEND;
        case TK_CAP_SHARE: return TK_CAP_SHARE;
        case TK_CAP_ANY: return TK_TAG;
        default: return TK_BOX;
      }
    }

    case TK_REF:
    case TK_REF_BIND:
      return cap;

    case TK_VAL:
    case TK_VAL_BIND:
    {
      switch(cap)
      {
        case TK_CAP_SEND: return TK_CAP_SHARE;
        case TK_CAP_SHARE: return TK_CAP_SHARE;
        case TK_CAP_ANY: return TK_CAP_SHARE;
        default: return TK_VAL;
      }
      break;
    }

    case TK_BOX:
    case TK_CAP_READ:
    case TK_BOX_BIND:
    case TK_CAP_READ_BIND:
    {
      switch(cap)
      {
        case TK_ISO: return TK_TAG;
        case TK_VAL: return TK_VAL;
        case TK_CAP_READ: return TK_BOX;
        case TK_CAP_SEND: return TK_TAG;
        case TK_CAP_SHARE: return TK_CAP_SHARE;
        case TK_CAP_ANY: return TK_TAG;
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
    case TK_BOX: return TK_BOX;
    case TK_TAG: return TK_TAG;
    case TK_CAP_READ: return TK_CAP_READ;
    case TK_CAP_SEND: return TK_CAP_SEND;
    case TK_CAP_SHARE: return TK_CAP_SHARE;
    case TK_CAP_ANY: return TK_CAP_ANY;

    // A typeparamref as a constraint.
    case TK_NONE: return TK_CAP_ANY;

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
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
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
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
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
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
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

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      return cap_from_constraint(right);
    }

    default: {}
  }

  assert(0);
  return TK_NONE;
}
