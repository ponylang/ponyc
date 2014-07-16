#include "viewpoint.h"
#include "assemble.h"
#include "cap.h"
#include <assert.h>

static ast_t* viewpoint_for_type(token_id view, ast_t* type, int cap_index)
{
  ast_t* cap = ast_childidx(type, cap_index);
  token_id tcap = ast_id(cap);
  token_id rcap = cap_viewpoint(view, tcap);

  if(tcap != rcap)
  {
    type = ast_dup(type);
    cap = ast_childidx(type, cap_index);
    ast_replace(&cap, ast_from(cap, rcap));
  }

  return type;
}

static ast_t* viewpoint_cap(token_id cap, ast_t* type)
{
  if(cap == TK_TAG)
    return NULL;

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
      ast_add(r_type, viewpoint_cap(cap, right));
      ast_add(r_type, viewpoint_cap(cap, left));
      return r_type;
    }

    case TK_NOMINAL:
      return viewpoint_for_type(cap, type, 3);

    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
      return viewpoint_for_type(cap, type, 1);

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* viewpoint(ast_t* left, ast_t* right)
{
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  // if the left side is 'this' and has box capability, we return an arrow type
  // to allow the receiver to type check for ref and val receivers as well.
  if((ast_id(left) == TK_THIS) && (ast_id(l_type) == TK_ARROW))
  {
    ast_t* arrow = ast_from(l_type, TK_ARROW);
    ast_add(arrow, r_type);
    ast_add(arrow, ast_from(l_type, TK_THISTYPE));
    return arrow;
  }

  return viewpoint_type(l_type, r_type);
}

ast_t* viewpoint_type(ast_t* l_type, ast_t* r_type)
{
  switch(ast_id(l_type))
  {
    case TK_NOMINAL:
    {
      token_id cap = ast_id(ast_childidx(l_type, 3));
      return viewpoint_cap(cap, r_type);
    }

    case TK_ARROW:
    {
      // a viewpoint type picks up another level of arrow type
      ast_t* ast = ast_dup(l_type);
      ast_t* child = ast_childidx(ast, 1);

      // arrow is right associative
      while(ast_id(child) == TK_ARROW)
        child = ast_childidx(child, 1);

      ast_t* arrow = ast_from(child, TK_ARROW);
      ast_add(arrow, r_type);
      ast_add(arrow, child);
      ast_replace(&child, arrow);

      return ast;
    }

    case TK_TYPEPARAMREF:
    {
      token_id cap = ast_id(ast_childidx(l_type, 1));

      // if the left side is a type parameter with no capability, return an
      // arrow type if the constraint is box, otherwise use the constraint cap.
      if(cap == TK_NONE)
      {
        ast_t* def = ast_data(l_type);
        ast_t* constraint = ast_childidx(def, 1);
        cap = cap_for_type(constraint);

        if(cap == TK_BOX)
        {
          ast_t* arrow = ast_from(l_type, TK_ARROW);
          ast_add(arrow, r_type);
          ast_add(arrow, l_type);
          return arrow;
        }
      }

      return viewpoint_cap(cap, r_type);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

void flatten_thistype(ast_t** astp, ast_t* type)
{
  ast_t* ast = *astp;
  assert(ast_id(type) == TK_NOMINAL);

  switch(ast_id(ast))
  {
    case TK_ARROW:
    {
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);

      if(ast_id(left) == TK_THISTYPE)
      {
        ast_t* cap = ast_childidx(type, 3);
        ast = viewpoint_cap(ast_id(cap), right);
        ast_replace(astp, ast);
        return;
      }
    }

    default: {}
  }

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    flatten_thistype(&child, type);
    child = ast_sibling(child);
  }
}

bool safe_to_write(ast_t* ast, ast_t* type)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_VARREF:
    case TK_PARAMREF:
      return true;

    case TK_FVARREF:
    case TK_FLETREF:
    {
      // if left is x.f, we need the type of x to determine safe to write
      ast_t* left = ast_child(ast);
      ast_t* l_type = ast_type(left);
      return cap_safetowrite(cap_for_type(l_type), cap_for_type(type));
    }

    case TK_TUPLE:
    {
      // safe to write if every component is safe to write
      assert(ast_id(type) == TK_TUPLETYPE);
      ast_t* child = ast_child(ast);
      ast_t* tchild = ast_child(type);

      while((child != NULL) && (tchild != NULL))
      {
        if(!safe_to_write(child, tchild))
          return false;

        child = ast_sibling(child);
        tchild = ast_sibling(tchild);
      }

      assert(child == NULL);
      assert(tchild == NULL);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}
