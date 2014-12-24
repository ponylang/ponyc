#include "alias.h"
#include "viewpoint.h"
#include "cap.h"
#include "../ast/token.h"
#include <assert.h>

static ast_t* alias_for_type(ast_t* type, int index)
{
  ast_t* ephemeral = ast_childidx(type, index + 1);

  switch(ast_id(ephemeral))
  {
    case TK_EPHEMERAL:
      type = ast_dup(type);
      ephemeral = ast_childidx(type, index + 1);
      ast_setid(ephemeral, TK_NONE);
      return type;

    case TK_NONE:
    {
      ast_t* cap = ast_childidx(type, index);

      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_NONE:
          type = ast_dup(type);
          ephemeral = ast_childidx(type, index + 1);
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

static ast_t* alias_bind_for_type(ast_t* type, int index)
{
  ast_t* cap = ast_childidx(type, index);

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
      // Prevent trn from binding to a box constraint.
      type = ast_dup(type);
      cap = ast_childidx(type, index);
      ast_setid(cap, TK_TAG);
      return type;

    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

static ast_t* recover_for_type(ast_t* type, int index)
{
  ast_t* cap = ast_childidx(type, index);
  token_id tcap = ast_id(cap);
  token_id rcap;

  switch(tcap)
  {
    case TK_ISO:
    case TK_VAL:
    case TK_TAG:
      return type;

    case TK_TRN:
    case TK_REF:
      rcap = TK_ISO;
      break;

    case TK_BOX:
      rcap = TK_VAL;
      break;

    default:
      assert(0);
      return NULL;
  }

  type = ast_dup(type);
  cap = ast_childidx(type, index);
  ast_setid(cap, rcap);
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
      return alias_for_type(type, 3);

    case TK_TYPEPARAMREF:
      return alias_for_type(type, 1);

    case TK_ARROW:
    {
      // alias just the right side. the left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, alias(right));
      ast_add(r_type, left);
      return r_type;
    }

    case TK_FUNTYPE:
      return type;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* alias_bind(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Alias each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, alias_bind(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
      return alias_bind_for_type(type, 3);

    case TK_TYPEPARAMREF:
      return alias_bind_for_type(type, 1);

    case TK_ARROW:
    {
      // Alias just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, alias_bind(right));
      ast_add(r_type, left);
      return r_type;
    }

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* infer(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_DONTCARE:
      return type;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Infer each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, infer(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    {
      ast_t* ephemeral = ast_childidx(type, 4);

      if(ast_id(ephemeral) == TK_BORROWED)
      {
        type = ast_dup(type);
        AST_GET_CHILDREN(type, pkg, id, typeparams, cap, eph);
        ast_setid(eph, TK_NONE);

        switch(ast_id(cap))
        {
          case TK_ISO: ast_setid(cap, TK_TAG); break;
          case TK_TRN: ast_setid(cap, TK_BOX); break;
          default: break;
        }
      }

      return type;
    }

    case TK_TYPEPARAMREF:
    {
      ast_t* ephemeral = ast_childidx(type, 2);

      if(ast_id(ephemeral) == TK_BORROWED)
      {
        type = ast_dup(type);
        AST_GET_CHILDREN(type, id, cap, eph);
        ast_setid(eph, TK_NONE);

        switch(ast_id(cap))
        {
          case TK_ISO: ast_setid(cap, TK_TAG); break;
          case TK_TRN: ast_setid(cap, TK_BOX); break;
          default: break;
        }
      }

      return type;
    }

    case TK_ARROW:
    {
      // Infer just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      AST_GET_CHILDREN(type, left, right);
      ast_add(r_type, infer(right));
      ast_add(r_type, left);
      return r_type;
    }

    case TK_FUNTYPE:
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
        ast_append(r_type, consume_type(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    {
      type = ast_dup(type);
      ast_t* ephemeral = ast_childidx(type, 4);

      switch(ast_id(ephemeral))
      {
        case TK_NONE:
          ast_setid(ephemeral, TK_EPHEMERAL);
          break;

        case TK_BORROWED:
          ast_setid(ephemeral, TK_NONE);
          break;

        default: {}
      }

      return type;
    }

    case TK_TYPEPARAMREF:
    {
      type = ast_dup(type);
      ast_t* ephemeral = ast_childidx(type, 2);

      switch(ast_id(ephemeral))
      {
        case TK_NONE:
          ast_setid(ephemeral, TK_EPHEMERAL);
          break;

        case TK_BORROWED:
          ast_setid(ephemeral, TK_NONE);
          break;

        default: {}
      }

      return type;
    }

    case TK_ARROW:
    {
      // Consume just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, consume_type(right));
      ast_add(r_type, left);
      return r_type;
    }

    case TK_FUNTYPE:
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
      // recover each element
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, recover_type(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
      return recover_for_type(type, 3);

    case TK_TYPEPARAMREF:
      return recover_for_type(type, 1);

    case TK_ARROW:
    {
      // recover just the right side. the left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, recover_type(right));
      ast_add(r_type, left);
      return r_type;
    }

    case TK_FUNTYPE:
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
      // Test the lower bounds.
      ast_t* lower = viewpoint_lower(ast_childidx(type, 1));
      bool ok = sendable(lower);
      ast_free_unattached(lower);
      return ok;
    }

    case TK_NOMINAL:
    {
      ast_t* cap = ast_childidx(type, 3);
      return cap_sendable(ast_id(cap));
    }

    case TK_TYPEPARAMREF:
    {
      ast_t* cap = ast_childidx(type, 1);
      return cap_sendable(ast_id(cap));
    }

    case TK_FUNTYPE:
      return true;

    default: {}
  }

  assert(0);
  return false;
}
