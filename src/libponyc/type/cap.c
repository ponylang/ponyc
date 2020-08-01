#include "cap.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include "ponyassert.h"

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

    case TK_ALIASED:
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
          // Alias as #share.
          *cap = TK_CAP_SHARE;
          break;

        case TK_CAP_ANY:
          // Alias as #alias.
          *cap = TK_CAP_ALIAS;
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

  if(super == TK_TAG)
    return true;

  // Every possible instantiation of sub must be a subtype of every possible
  // instantiation of super.
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
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_SEND:  // {iso, val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
        case TK_CAP_ANY:   // {iso, trn, ref, val, box, tag}
          return true;

        default: {}
      }
      break;

    case TK_TRN:
      switch(super)
      {
        case TK_TRN:
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(super)
      {
        case TK_REF:
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(super)
      {
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_SHARE: // {val, tag}
          return true;

        default: {}
      }
      break;

    case TK_BOX:
    case TK_CAP_READ: // {ref, val, box}
      switch(super)
      {
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    default: {}
  }

  return false;
}

bool is_cap_sub_cap_constraint(token_id sub, token_id subalias, token_id super,
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

  if((sub == super) || (super == TK_CAP_ANY))
    return true;

  // Every possible instantiation of sub must be a member of the set of every
  // possible instantiation of super.
  switch(super)
  {
    case TK_CAP_READ: // {ref, val, box}
      switch(sub)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    case TK_CAP_SHARE: // {val, tag}
      switch(sub)
      {
        case TK_VAL:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_CAP_SEND: // {iso, val, tag}
      switch(sub)
      {
        case TK_ISO:
        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SHARE: // {val, tag}
          return true;

        default: {}
      }
      break;

    case TK_CAP_ALIAS: // {ref, val, box, tag}
      switch(sub)
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
          return true;

        default: {}
      }
      break;

    default: {}
  }

  return false;
}

bool is_cap_sub_cap_bound(token_id sub, token_id subalias, token_id super,
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

  if((sub == super) || (super == TK_TAG))
    return true;

  // Sub and super share the same initial bounds.
  //
  // If either rcap is a specific/singular capability, use the same rule
  // as in is_cap_sub_cap: every possible instantiation of sub must be a
  // subtype of every possible instantiation of super.
  //
  // If both rcaps are generic/set capabilities, use the following rule:
  // every instantiation of the super rcap must be a supertype of some
  // instantiation of the sub rcap (but not necessarily the same instantiation
  // of the sub rcap).
  switch(sub)
  {
    case TK_ISO:
      switch(super)
      {
        case TK_TRN:
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_SEND:  // {iso, val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
        case TK_CAP_ANY:   // {iso, trn, ref, val, box, tag}
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
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(super)
      {
        case TK_BOX:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(super)
      {
        case TK_BOX:
        case TK_CAP_SHARE: // {val, tag}
          return true;

        default: {}
      }
      break;

    case TK_CAP_READ: // {ref, val, box}
      switch(super)
      {
        case TK_BOX:
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
          return true;

        default: {}
      }
      break;

    case TK_CAP_SEND: // {iso, val, tag}
      switch(super)
      {
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
        case TK_CAP_ANY:   // {iso, trn, ref, val, box, tag}
          return true;

        default: {}
      }
      break;

    case TK_CAP_ALIAS: // {ref, val, box, tag}
      switch(super)
      {
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
          return true;

        default: {}
      }
      break;

    case TK_CAP_ANY: // {iso, trn, ref, val, box, tag}
      switch(super)
      {
        case TK_CAP_READ:  // {ref, val, box}
        case TK_CAP_SHARE: // {val, tag}
        case TK_CAP_SEND:  // {iso, val, tag}
        case TK_CAP_ALIAS: // {ref, val, box, tag}
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

  if((left_cap == TK_TAG) || (right_cap == TK_TAG))
    return true;

  // Every possible instantiation of the left cap must be compatible with every
  // possible instantiation of right cap.
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
        case TK_CAP_SHARE:
        case TK_CAP_ALIAS:
          return true;

        default: {}
      }
      break;

    case TK_CAP_READ:
    case TK_CAP_ALIAS:
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

  pony_assert(0);
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

token_id cap_dispatch(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      // Use the least specific capability.
      ast_t* child = ast_child(type);
      token_id cap = cap_dispatch(child);
      child = ast_sibling(child);

      while(child != NULL)
      {
        token_id child_cap = cap_dispatch(child);

        if(is_cap_sub_cap(cap, TK_NONE, child_cap, TK_NONE))
          cap = child_cap;
        else if(!is_cap_sub_cap(child_cap, TK_NONE, cap, TK_NONE))
          cap = TK_BOX;

        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_ISECTTYPE:
    {
      // Use the most specific capability. Because all capabilities will be
      // locally compatible, there is a subtype relationship.
      ast_t* child = ast_child(type);
      token_id cap = cap_dispatch(child);
      child = ast_sibling(child);

      while(child != NULL)
      {
        token_id child_cap = cap_dispatch(child);

        if(is_cap_sub_cap(child_cap, TK_NONE, cap, TK_NONE))
          cap = child_cap;

        child = ast_sibling(child);
      }

      return cap;
    }

    case TK_NOMINAL:
      return cap_single(type);

    default: {}
  }

  pony_assert(0);
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

bool cap_view_upper(token_id left_cap, token_id left_eph,
  token_id* right_cap, token_id* right_eph)
{
  cap_aliasing(&left_cap, &left_eph);
  cap_aliasing(right_cap, right_eph);

  // Can't see through these viewpoints.
  switch(left_cap)
  {
    case TK_TAG:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ALIAS:
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
        case TK_CAP_SEND:
          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_ALIAS:
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
        case TK_CAP_ALIAS:
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
        case TK_CAP_ALIAS:
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
      pony_assert(0);
      return false;
  }

  return true;
}

bool cap_view_lower(token_id left_cap, token_id left_eph,
  token_id* right_cap, token_id* right_eph)
{
  cap_aliasing(&left_cap, &left_eph);
  cap_aliasing(right_cap, right_eph);

  // Can't see through these viewpoints.
  switch(left_cap)
  {
    case TK_TAG:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ALIAS:
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
        case TK_CAP_ALIAS:
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
        case TK_CAP_SEND:
          if(left_eph == TK_EPHEMERAL)
            *right_eph = TK_EPHEMERAL;
          break;

        case TK_VAL:
        case TK_CAP_SHARE:
          break;

        case TK_CAP_READ:
        case TK_CAP_ALIAS:
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
        case TK_CAP_ALIAS:
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
        case TK_CAP_ALIAS:
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
      pony_assert(0);
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

bool cap_immutable_or_opaque(token_id cap)
{
  switch(cap)
  {
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
    case TK_CAP_SHARE:
      return true;

    default: {}
  }

  return false;
}

bool cap_safetomove(token_id into, token_id cap, direction direction)
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
        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SEND:
        case TK_CAP_SHARE:
          return true;

        case TK_TRN:
          switch (direction) {
            case WRITE: return true;
            case EXTRACT: return false;
          }
        case TK_BOX:
          switch (direction) {
            case WRITE: return false;
            case EXTRACT: return true;
          }

        default: {}
      }
      break;

    case TK_REF:
      return true;

    default: {}
  }

  return false;
}
