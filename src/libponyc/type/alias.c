#include "alias.h"
#include <assert.h>

static token_id alias_for_cap(token_id id)
{
  switch(id)
  {
    case TK_ISO: return TK_TAG;
    case TK_TRN: return TK_BOX;
    case TK_REF: return TK_REF;
    case TK_VAL: return TK_VAL;
    case TK_BOX: return TK_BOX;
    case TK_TAG: return TK_TAG;
    case TK_NONE: return TK_NONE; // TODO: figure out default capability?
    default: {}
  }

  assert(0);
  return TK_NONE;
}

static ast_t* alias_for_type(ast_t* type, int cap_index, int eph_index)
{
  ast_t* ephemeral = ast_childidx(type, eph_index);

  if(ast_id(ephemeral) == TK_HAT)
  {
    // ephemeral capability becomes non-ephemeral
    type = ast_dup(type);
    ephemeral = ast_childidx(type, eph_index);
    ast_replace(ephemeral, ast_from(type, TK_NONE));
  } else {
    // non-ephemeral capability gets aliased
    ast_t* cap = ast_childidx(type, cap_index);
    token_id tcap = ast_id(cap);
    token_id acap = alias_for_cap(tcap);

    if(tcap != acap)
    {
      type = ast_dup(type);
      cap = ast_childidx(type, cap_index);
      ast_replace(cap, ast_from(type, acap));
    }
  }

  return type;
}

ast_t* alias(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // alias each side
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, alias(left));
      ast_add(r_type, alias(right));
      return r_type;
    }

    case TK_NOMINAL:
      return alias_for_type(type, 3, 4);

    case TK_STRUCTURAL:
      return alias_for_type(type, 1, 2);

    case TK_ARROW:
      // TODO: alias with viewpoint adaptation
      return type;

    case TK_THISTYPE:
    case TK_ERROR:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}
