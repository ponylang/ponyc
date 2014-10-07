#include "alias.h"
#include "cap.h"
#include "../ast/token.h"
#include <assert.h>

static ast_t* alias_for_type(ast_t* type, int index)
{
  ast_t* ephemeral = ast_childidx(type, index + 1);

  if(ast_id(ephemeral) == TK_HAT)
  {
    // ephemeral capability becomes non-ephemeral
    type = ast_dup(type);
    ephemeral = ast_childidx(type, index + 1);
    ast_replace(&ephemeral, ast_from(type, TK_NONE));
  } else {
    // non-ephemeral capability gets aliased
    ast_t* cap = ast_childidx(type, index);
    token_id tcap = ast_id(cap);
    token_id acap = cap_alias(tcap);

    if(tcap != acap)
    {
      type = ast_dup(type);
      cap = ast_childidx(type, index);
      ast_replace(&cap, ast_from(type, acap));
    }
  }

  return type;
}

static ast_t* recover_for_type(ast_t* type, int index)
{
  ast_t* cap = ast_childidx(type, index);
  token_id tcap = ast_id(cap);
  token_id rcap = cap_recover(tcap);

  if(tcap != rcap)
  {
    type = ast_dup(type);
    cap = ast_childidx(type, index);
    ast_replace(&cap, ast_from(cap, rcap));
  }

  return type;
}

static ast_t* send_for_type(ast_t* type, int index)
{
  ast_t* cap = ast_childidx(type, index);
  token_id tcap = ast_id(cap);
  token_id rcap = cap_send(tcap);

  if(tcap != rcap)
  {
    type = ast_dup(type);
    cap = ast_childidx(type, index);
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

    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
      return alias_for_type(type, 1);

    case TK_INTLITERAL:
    case TK_FLOATLITERAL:
      return ast_dup(type);

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
      // consume each element
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
      ast_replace(&ephemeral, ast_from(ephemeral, TK_HAT));
      return type;
    }

    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
    {
      type = ast_dup(type);
      ast_t* ephemeral = ast_childidx(type, 2);
      ast_replace(&ephemeral, ast_from(ephemeral, TK_HAT));
      return type;
    }

    case TK_ARROW:
    {
      // consume just the right side. the left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, consume_type(right));
      ast_add(r_type, left);
      return r_type;
    }

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

    case TK_STRUCTURAL:
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

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* send_type(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Send type for each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, send_type(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
      return send_for_type(type, 3);

    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
      return send_for_type(type, 1);

    case TK_ARROW:
    {
      // Send type just for the right side. Left side is either 'this' or a type
      // parameter, and stays the same.
      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      ast_add(r_type, send_type(right));
      ast_add(r_type, left);
      return r_type;
    }

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
      // test each element
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!sendable(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_INTLITERAL:
    case TK_FLOATLITERAL:
      return true;

    case TK_NOMINAL:
    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
    case TK_ARROW:
      return cap_sendable(cap_for_type(type));

    default: {}
  }

  assert(0);
  return false;
}
