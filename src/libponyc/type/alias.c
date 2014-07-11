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
    ast_replace(&ephemeral, ast_from(type, TK_NONE));
  } else {
    // non-ephemeral capability gets aliased
    ast_t* cap = ast_childidx(type, cap_index);
    token_id tcap = ast_id(cap);
    token_id acap = alias_for_cap(tcap);

    if(tcap != acap)
    {
      type = ast_dup(type);
      cap = ast_childidx(type, cap_index);
      ast_replace(&cap, ast_from(type, acap));
    }
  }

  return type;
}

static token_id recover_for_cap(token_id id)
{
  switch(id)
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

static ast_t* recover_for_type(ast_t* type, int cap_index)
{
  ast_t* cap = ast_childidx(type, cap_index);
  token_id tcap = ast_id(cap);
  token_id rcap = recover_for_cap(tcap);

  if(tcap != rcap)
  {
    type = ast_dup(type);
    cap = ast_childidx(type, cap_index);
    ast_replace(&cap, ast_from(cap, rcap));
  }

  return type;
}

static token_id viewpoint_for_cap(token_id view, token_id cap)
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

    case TK_TAG:
      return TK_NONE;

    default: {}
  }

  assert(0);
  return TK_NONE;
}

static ast_t* viewpoint_for_type(token_id view, ast_t* type, int cap_index)
{
  ast_t* cap = ast_childidx(type, cap_index);
  token_id tcap = ast_id(cap);
  token_id rcap = viewpoint_for_cap(view, tcap);

  if(rcap == TK_NONE)
    return NULL;

  if(tcap != rcap)
  {
    type = ast_dup(type);
    cap = ast_childidx(type, cap_index);
    ast_replace(&cap, ast_from(cap, rcap));
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
      ast_add(r_type, alias(right));
      ast_add(r_type, alias(left));
      return r_type;
    }

    case TK_NOMINAL:
      return alias_for_type(type, 3, 4);

    case TK_STRUCTURAL:
      return alias_for_type(type, 1, 2);

    case TK_ARROW:
      // TODO: alias with viewpoint adaptation
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* consume_type(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // consume each side
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, consume_type(right));
      ast_add(r_type, consume_type(left));
      return r_type;
    }

    case TK_NOMINAL:
    {
      type = ast_dup(type);
      ast_t* ephemeral = ast_childidx(type, 4);
      ast_replace(&ephemeral, ast_from(ephemeral, TK_HAT));
      return type;
    }

    case TK_STRUCTURAL:
    {
      type = ast_dup(type);
      ast_t* ephemeral = ast_childidx(type, 2);
      ast_replace(&ephemeral, ast_from(ephemeral, TK_HAT));
      return type;
    }

    case TK_ARROW:
      // TODO: consume with viewpoint adaptation
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* recover_type(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // recover each side
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, recover_type(right));
      ast_add(r_type, recover_type(left));
      return r_type;
    }

    case TK_NOMINAL:
      return recover_for_type(type, 3);

    case TK_STRUCTURAL:
      return recover_for_type(type, 1);

    case TK_ARROW:
      // TODO: recover with viewpoint adaptation
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* viewpoint(token_id cap, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // adapt each side
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, viewpoint(cap, right));
      ast_add(r_type, viewpoint(cap, left));
      return r_type;
    }

    case TK_NOMINAL:
      return viewpoint_for_type(cap, type, 3);

    case TK_STRUCTURAL:
      return viewpoint_for_type(cap, type, 1);

    case TK_ARROW:
      // TODO: viewpoint adaptation
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}
