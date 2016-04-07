#include "cap.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include <assert.h>

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
          // Alias as tag.
          *cap = TK_TAG;
          break;

        case TK_TRN:
          // Alias as box.
          *cap = TK_BOX;
          break;

        case TK_CAP_SEND:
          // Alias as share.
          *cap = TK_CAP_SHARE;
          break;

        case TK_CAP_ANY:
          // TODO: should not be tag
          // Alias as tag.
          *cap = TK_TAG;
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

        case TK_CAP_ANY_BIND:
          // TODO: should not be tag
          *cap = TK_TAG_BIND;
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

bool is_cap_match_cap(token_id operand_cap, token_id operand_eph,
  token_id pattern_cap, token_id pattern_eph)
{
  // Transform the cap based on the aliasing info.
  cap_aliasing(&operand_cap, &operand_eph);
  cap_aliasing(&pattern_cap, &pattern_eph);

  if(pattern_eph == TK_EPHEMERAL)
  {
    // Operand must be ephemeral.
    if(operand_eph != TK_EPHEMERAL)
      return false;
  }

  operand_cap = cap_unbind(operand_cap);
  pattern_cap = cap_unbind(pattern_cap);

  if((operand_cap == pattern_cap) || (pattern_cap == TK_TAG))
    return true;

  // Some instantiaion of the operand refcap must be a subtype of some
  // instantiation of the pattern refcap.
  switch(operand_cap)
  {
    case TK_ISO:
    case TK_CAP_SEND:
    case TK_CAP_ANY:
      switch(pattern_cap)
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
          return true;

        default: {}
      }
      break;

    case TK_TRN:
      switch(pattern_cap)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(pattern_cap)
      {
        case TK_BOX:
        case TK_CAP_READ:
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
    case TK_CAP_SHARE:
      switch(pattern_cap)
      {
        case TK_BOX:
        case TK_CAP_READ:
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          return true;

        default: {}
      }
      break;

    case TK_BOX:
      switch(pattern_cap)
      {
        case TK_CAP_READ:
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          return true;

        default: {}
      }
      break;

    case TK_TAG:
      switch(pattern_cap)
      {
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          return true;

        default: {}
      }
      break;

    case TK_CAP_READ:
      switch(pattern_cap)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_SHARE:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
          return true;

        default: {}
      }
      break;

    default: {}
  }

  return false;
}

bool is_cap_compat_cap(token_id left_cap, token_id left_eph,
  token_id right_cap, token_id right_eph)
{
  // Transform the cap based on the aliasing info.
  cap_aliasing(&left_cap, &left_eph);
  cap_aliasing(&right_cap, &right_eph);

  // Ignore compatibility for bind caps.
  if((cap_unbind(left_cap) != left_cap) ||
    (cap_unbind(right_cap) != right_cap))
    return true;

  if((left_cap == TK_TAG) || (right_cap == TK_TAG))
    return true;

  // Every possible value of left must be compatible with every possible value
  // of right.
  switch(left_cap)
  {
    case TK_ISO:
      switch(right_cap)
      {
        case TK_ISO:
          return true;

        default: {}
      }
      break;

    case TK_TRN:
      switch(right_cap)
      {
        case TK_TRN:
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(right_cap)
      {
        case TK_REF:
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(right_cap)
      {
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_SHARE:
          return true;

        default: {}
      }
      break;

    case TK_BOX:
      switch(right_cap)
      {
        case TK_TRN:
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:
          return true;

        default: {}
      }
      break;

    case TK_CAP_READ:
      switch(right_cap)
      {
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    case TK_CAP_SEND:
      break;

    case TK_CAP_SHARE:
      switch(right_cap)
      {
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_SHARE:
          return true;

        default: {}
      }
      break;

    default: {}
  }

  return false;
}

ast_t* cap_fetch(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      return ast_childidx(type, 3);

    case TK_TYPEPARAMREF:
      return ast_childidx(type, 1);

    default: {}
  }

  assert(0);
  return NULL;
}

token_id cap_single(ast_t* type)
{
  ast_t* cap = cap_fetch(type);
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

token_id cap_bind(token_id cap)
{
  switch(cap)
  {
    case TK_ISO: return TK_ISO_BIND;
    case TK_TRN: return TK_TRN_BIND;
    case TK_REF: return TK_REF_BIND;
    case TK_VAL: return TK_VAL_BIND;
    case TK_BOX: return TK_BOX_BIND;
    case TK_TAG: return TK_TAG_BIND;
    case TK_CAP_READ: return TK_CAP_READ_BIND;
    case TK_CAP_SEND: return TK_CAP_SEND_BIND;
    case TK_CAP_SHARE: return TK_CAP_SHARE_BIND;
    case TK_CAP_ANY: return TK_CAP_ANY_BIND;

    default: {}
  }

  assert(0);
  return cap;
}

token_id cap_unbind(token_id cap)
{
  switch(cap)
  {
    case TK_ISO_BIND: return TK_ISO;
    case TK_TRN_BIND: return TK_TRN;
    case TK_REF_BIND: return TK_REF;
    case TK_VAL_BIND: return TK_VAL;
    case TK_BOX_BIND: return TK_BOX;
    case TK_TAG_BIND: return TK_TAG;
    case TK_CAP_READ_BIND: return TK_CAP_READ;
    case TK_CAP_SEND_BIND: return TK_CAP_SEND;
    case TK_CAP_SHARE_BIND: return TK_CAP_SHARE;
    case TK_CAP_ANY_BIND: return TK_CAP_ANY;

    default: {}
  }

  return cap;
}

bool cap_view_upper(token_id left_cap, token_id left_eph,
  token_id* right_cap, token_id* right_eph)
{
  left_cap = cap_unbind(left_cap);
  *right_cap = cap_unbind(*right_cap);

  cap_aliasing(&left_cap, &left_eph);
  cap_aliasing(right_cap, right_eph);

  // Can't see through these viewpoints.
  switch(left_cap)
  {
    case TK_TAG:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ANY:
      return false;

    default: {}
  }

  // A tag is always seen as a tag.
  if(*right_cap == TK_TAG)
    return true;

  switch(left_cap)
  {
    case TK_ISO:
    {
      switch(*right_cap)
      {
        case TK_ISO:
        case TK_CAP_SEND:
          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        default:
          *right_cap = TK_TAG;
          *right_eph = TK_NONE;
      }
      break;
    }

    case TK_TRN:
    {
      switch(*right_cap)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_CAP_SEND:
          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_ANY:
          *right_cap = TK_TAG;
          *right_eph = TK_NONE;
          break;

        default:
          *right_cap = TK_BOX;
          *right_eph = TK_NONE;
      }
      break;
    }

    case TK_REF:
      break;

    case TK_VAL:
    {
      switch(*right_cap)
      {
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
        case TK_CAP_ANY:
          *right_cap = TK_CAP_SHARE;
          break;

        default:
          *right_cap = TK_VAL;
      }

      *right_eph = TK_NONE;
      break;
    }

    case TK_BOX:
    case TK_CAP_READ:
    {
      switch(*right_cap)
      {
        case TK_ISO:
          *right_cap = TK_TAG;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_SEND:
        case TK_CAP_ANY:
          *right_cap = TK_TAG;
          break;

        default:
          *right_cap = TK_BOX;
      }

      *right_eph = TK_NONE;
      break;
    }

    default:
      assert(0);
      return false;
  }

  return true;
}

bool cap_view_lower(token_id left_cap, token_id left_eph,
  token_id* right_cap, token_id* right_eph)
{
  left_cap = cap_unbind(left_cap);
  *right_cap = cap_unbind(*right_cap);

  cap_aliasing(&left_cap, &left_eph);
  cap_aliasing(right_cap, right_eph);

  // Can't see through these viewpoints.
  switch(left_cap)
  {
    case TK_TAG:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ANY:
      return false;

    default: {}
  }

  // A tag is always seen as a tag.
  if(*right_cap == TK_TAG)
    return true;

  switch(left_cap)
  {
    case TK_ISO:
    {
      switch(*right_cap)
      {
        case TK_ISO:
        case TK_CAP_SEND:
          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_READ:
        case TK_CAP_ANY:
          *right_cap = TK_CAP_SEND;

          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        default:
          *right_cap = TK_TAG;
          *right_eph = TK_NONE;
      }
      break;
    }

    case TK_TRN:
    {
      switch(*right_cap)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_CAP_SEND:
          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_READ:
          *right_cap = TK_VAL;
          *right_eph = TK_NONE;
          break;

        case TK_CAP_ANY:
          *right_cap = TK_ISO;

          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        default:
          *right_cap = TK_BOX;
          *right_eph = TK_NONE;
      }
      break;
    }

    case TK_REF:
      break;

    case TK_VAL:
    {
      switch(*right_cap)
      {
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
        case TK_CAP_ANY:
          *right_cap = TK_CAP_SHARE;
          break;

        default:
          *right_cap = TK_VAL;
      }

      *right_eph = TK_NONE;
      break;
    }

    case TK_BOX:
    {
      switch(*right_cap)
      {
        case TK_ISO:
          *right_cap = TK_TAG;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_SEND:
          *right_cap = TK_CAP_SHARE;
          break;

        case TK_CAP_READ:
        case TK_CAP_ANY:
          *right_cap = TK_VAL;
          break;

        default:
          *right_cap = TK_BOX;
      }

      *right_eph = TK_NONE;
      break;
    }

    case TK_CAP_READ:
    {
      switch(*right_cap)
      {
        case TK_ISO:
          *right_cap = TK_CAP_SEND;
          break;

        case TK_REF:
          *right_cap = TK_CAP_READ;
          break;

        case TK_BOX:
          *right_cap = TK_VAL;
          break;

        default: {}
      }
      break;
    }

    default:
      assert(0);
      return false;
  }

  return true;
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
