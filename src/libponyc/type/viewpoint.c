#include "viewpoint.h"
#include "assemble.h"
#include "cap.h"
#include <assert.h>

static ast_t* make_arrow_type(ast_t* left, ast_t* right)
{
  switch(ast_id(right))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* type = ast_from(right, ast_id(right));
      ast_t* child = ast_child(right);

      while(child != NULL)
      {
        ast_append(type, make_arrow_type(left, child));
        child = ast_sibling(child);
      }

      return type;
    }

    case TK_NOMINAL:
    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
    case TK_ARROW:
    {
      ast_t* arrow = ast_from(left, TK_ARROW);
      ast_add(arrow, right);
      ast_add(arrow, left);
      return arrow;
    }

    default: {}
  }

  assert(0);
  return NULL;
}

static ast_t* viewpoint_lower_for_type(ast_t* type, int cap_index)
{
  ast_t* cap = ast_childidx(type, cap_index);
  token_id tcap = ast_id(cap);
  token_id rcap = cap_viewpoint_lower(tcap);

  if(tcap != rcap)
  {
    type = ast_dup(type);
    cap = ast_childidx(type, cap_index);
    ast_replace(&cap, ast_from(cap, rcap));
  }

  return type;
}

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
      // adapt all elements
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, viewpoint_cap(cap, child));
        child = ast_sibling(child);
      }

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
    return make_arrow_type(ast_from(l_type, TK_THISTYPE), r_type);

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

      ast_t* view = viewpoint_type(child, r_type);
      ast_replace(&child, view);
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
          return make_arrow_type(l_type, r_type);
      }

      return viewpoint_cap(cap, r_type);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* viewpoint_upper(ast_t* type)
{
  if(ast_id(type) == TK_ARROW)
  {
    ast_t* right = ast_childidx(type, 1);
    ast_t* r_right = viewpoint_upper(right);
    return viewpoint_cap(TK_BOX, r_right);
  }

  return type;
}

ast_t* viewpoint_lower(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // adapt all elements
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, viewpoint_lower(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
      return viewpoint_lower_for_type(type, 3);

    case TK_STRUCTURAL:
    case TK_TYPEPARAMREF:
      return viewpoint_lower_for_type(type, 1);

    case TK_ARROW:
    {
      ast_t* right = ast_childidx(type, 1);
      return viewpoint_lower(right);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

void flatten_thistype(ast_t** astp, ast_t* type)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_ARROW:
    {
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);

      if(ast_id(left) == TK_THISTYPE)
      {
        ast_t* r_right = viewpoint_type(type, right);
        ast_replace(astp, r_right);
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
      switch(ast_id(type))
      {
        case TK_UNIONTYPE:
        case TK_ISECTTYPE:
        case TK_TUPLETYPE:
        {
          // safe to write if every component is safe to write
          ast_t* child = ast_child(type);

          while(child != NULL)
          {
            if(!safe_to_write(ast, child))
              return false;

            child = ast_sibling(child);
          }

          return true;
        }

        case TK_NOMINAL:
        case TK_STRUCTURAL:
        case TK_TYPEPARAMREF:
        case TK_ARROW:
        {
          // if left is x.f, we need the type of x to determine safe to write
          ast_t* left = ast_child(ast);
          ast_t* l_type = ast_type(left);
          token_id l_cap = cap_for_type(l_type);

          return cap_safetowrite(l_cap, cap_for_type(type));
        }

        default: {}
      }
      break;
    }

    default: {}
  }

  assert(0);
  return false;
}
