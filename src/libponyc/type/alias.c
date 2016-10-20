#include "alias.h"
#include "viewpoint.h"
#include "cap.h"
#include "../ast/token.h"
#include "../ast/astbuild.h"
#include <assert.h>

static ast_t* alias_single(ast_t* type)
{
  ast_t* cap = cap_fetch(type);
  ast_t* ephemeral = ast_sibling(cap);

  switch(ast_id(ephemeral))
  {
    case TK_EPHEMERAL:
      type = ast_dup(type);
      ephemeral = ast_sibling(cap_fetch(type));
      ast_setid(ephemeral, TK_NONE);
      return type;

    case TK_NONE:
    {
      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
        case TK_NONE:
          type = ast_dup(type);
          ephemeral = ast_sibling(cap_fetch(type));
          ast_setid(ephemeral, TK_BORROWED);
          break;

        default: {}
      }

      return type;
    }

    case TK_BORROWED:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

static ast_t* recover_single(ast_t* type, token_id rcap)
{
  ast_t* cap = cap_fetch(type);
  token_id tcap = ast_id(cap);
  bool rec_eph = false;

  switch(tcap)
  {
    case TK_ISO:
    {
      if(rcap == TK_NONE)
        rcap = TK_ISO;

      // If ephemeral, any rcap is acceptable. Otherwise, only iso or tag is.
      // This is because the iso may have come from outside the recover
      // expression.
      ast_t* eph = ast_sibling(cap);

      if((ast_id(eph) != TK_EPHEMERAL) && (rcap != TK_ISO) && (rcap != TK_TAG))
        return NULL;
      break;
    }

    case TK_TRN:
    case TK_REF:
      // These recover as iso, so any rcap is acceptable. We also set the rcap
      // as ephemeral to allow direct recovery to iso or trn.
      if(rcap == TK_NONE)
        rcap = TK_ISO;
      rec_eph = true;
      break;

    case TK_VAL:
    case TK_BOX:
    case TK_CAP_READ:
      // These recover as val, so mutable rcaps are not acceptable.
      if(rcap == TK_NONE)
        rcap = TK_VAL;

      switch(rcap)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_REF:
        case TK_CAP_READ:
        case TK_CAP_SEND:
        case TK_CAP_ALIAS:
        case TK_CAP_ANY:
          return NULL;

        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_CAP_SHARE:
          break;

        default:
          assert(0);
          return NULL;
      }
      break;

    case TK_TAG:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ALIAS:
    case TK_CAP_ANY:
      // Recovers as tag or itself only.
      if(rcap == TK_NONE)
        rcap = tcap;

      if((rcap != TK_TAG) && (rcap != tcap))
        return NULL;
      break;

    default:
      assert(0);
      return NULL;
  }

  type = ast_dup(type);
  cap = cap_fetch(type);
  ast_setid(cap, rcap);
  if(rec_eph)
    ast_setid(ast_sibling(cap), TK_EPHEMERAL);
  return type;
}

static ast_t* consume_single(ast_t* type, token_id ccap)
{
  type = ast_dup(type);
  ast_t* cap = cap_fetch(type);
  ast_t* eph = ast_sibling(cap);
  token_id tcap = ast_id(cap);

  switch(ast_id(eph))
  {
    case TK_NONE:
      ast_setid(eph, TK_EPHEMERAL);
      break;

    case TK_BORROWED:
      if(ccap == TK_BORROWED)
        ast_setid(eph, TK_NONE);
      break;

    default: {}
  }

  switch(ccap)
  {
    case TK_NONE:
    case TK_BORROWED:
      ccap = tcap;
      break;

    default: {}
  }

  if(!is_cap_sub_cap(tcap, TK_NONE, ccap, TK_NONE))
  {
    ast_free_unattached(type);
    return NULL;
  }

  ast_setid(cap, ccap);
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
      // alias each element
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, alias(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return alias_single(type);

    case TK_ARROW:
    {
      // Alias just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);

      BUILD(r_type, type,
        NODE(TK_ARROW,
          TREE(left)
          TREE(alias(right))));

      return r_type;
    }

    case TK_LAMBDATYPE:
    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* bind_type(ast_t* type)
{
  // A bind type is only ever used when checking constraints.
  // We change the capabilities to a bind capability. For example:
  // ref <: #read when binding, but not when assigning.
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      // Bind each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, bind_type(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      type = ast_dup(type);
      ast_t* cap = cap_fetch(type);
      ast_setid(cap, cap_bind(ast_id(cap)));
      return type;
    }

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* consume_type(ast_t* type, token_id cap)
{
  switch(ast_id(type))
  {
    case TK_DONTCARE:
      return type;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Consume each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* r_right = consume_type(child, cap);

        if(r_right == NULL)
        {
          ast_free_unattached(r_type);
          return NULL;
        }

        ast_append(r_type, r_right);
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return consume_single(type, cap);

    case TK_ARROW:
    {
      // Consume just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);

      BUILD(r_type, type,
        NODE(TK_ARROW,
        TREE(left)
        TREE(consume_type(right, cap))));

      return r_type;
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

static ast_t* recover_complex(ast_t* type, token_id cap)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
      break;

    default:
      assert(false);
      break;
  }

  // recover each element
  ast_t* r_type = ast_from(type, ast_id(type));
  ast_t* child = ast_child(type);

  while(child != NULL)
  {
    ast_t* r_right = recover_type(child, cap);

    if(r_right == NULL)
    {
      ast_free_unattached(r_type);
      return NULL;
    }

    ast_append(r_type, r_right);
    child = ast_sibling(child);
  }

  return r_type;
}

ast_t* recover_type(ast_t* type, token_id cap)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return recover_complex(type, cap);

    case TK_TUPLETYPE:
    {
      if((cap == TK_VAL) || (cap == TK_BOX) || (cap == TK_TAG))
        return recover_complex(type, cap);

      if(immutable_or_opaque(type))
        return recover_complex(type, cap);

      // If we're recovering to something writable, we can't lift the reference
      // capability because the objects in the tuple might alias each other.
      return ast_dup(type);
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return recover_single(type, cap);

    case TK_ARROW:
    {
      // recover just the right side. the left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);
      ast_t* r_right = recover_type(right, cap);

      if(r_right == NULL)
        return NULL;

      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_add(r_type, r_right);
      ast_add(r_type, left);
      return r_type;
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

bool sendable(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Test each element.
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!sendable(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      ast_t* lower = viewpoint_lower(type);

      if(lower == NULL)
        return false;

      bool ok = sendable(lower);
      ast_free_unattached(lower);
      return ok;
    }

    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, pkg, id, typeparams, cap, eph);
      return cap_sendable(ast_id(cap));
    }

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id, cap, eph);
      return cap_sendable(ast_id(cap));
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return false;

    default: {}
  }

  assert(0);
  return false;
}

bool immutable_or_opaque(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Test each element.
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!immutable_or_opaque(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      ast_t* lower = viewpoint_lower(type);

      if(lower == NULL)
        return false;

      bool ok = immutable_or_opaque(lower);
      ast_free_unattached(lower);
      return ok;
    }

    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, pkg, id, typeparams, cap, eph);
      return cap_immutable_or_opaque(ast_id(cap));
    }

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id, cap, eph);
      return cap_immutable_or_opaque(ast_id(cap));
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return false;

    default: {}
  }

  assert(0);
  return false;
}
