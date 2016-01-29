#include "safeto.h"
#include "cap.h"
#include "viewpoint.h"
#include <assert.h>

static bool safe_field_write(token_id cap, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Safe to write if every component is safe to write.
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!safe_field_write(cap, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      // Safe to write if the upper bounds is safe to write.
      ast_t* upper = viewpoint_upper(type);

      if(upper == NULL)
        return false;

      bool ok = safe_field_write(cap, upper);

      if(upper != type)
        ast_free_unattached(upper);

      return ok;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return cap_safetowrite(cap, cap_single(type));

    default: {}
  }

  assert(0);
  return false;
}

bool safe_to_write(ast_t* ast, ast_t* type)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_VARREF:
    case TK_DONTCARE:
      return true;

    case TK_FVARREF:
    case TK_FLETREF:
    case TK_EMBEDREF:
    {
      // If the ast is x.f, we need the type of x, which will be a nominal
      // type or an arrow type, since we were able to lookup a field on it.
      AST_GET_CHILDREN(ast, left, right);
      ast_t* l_type = ast_type(left);

      // Any viewpoint adapted type will not be safe to write to.
      if(ast_id(l_type) != TK_NOMINAL)
        return false;

      token_id l_cap = cap_single(l_type);

      // If the RHS is safe to write, we're done.
      if(safe_field_write(l_cap, type))
        return true;

      // If the field type (without adaptation) is safe, then it's ok as
      // well. So iso.tag = ref should be allowed.
      ast_t* r_type = ast_type(right);
      return safe_field_write(l_cap, r_type);
    }

    case TK_TUPLE:
    {
      // At this point, we know these will be the same length.
      assert(ast_id(type) == TK_TUPLETYPE);
      ast_t* child = ast_child(ast);
      ast_t* type_child = ast_child(type);

      while(child != NULL)
      {
        if(!safe_to_write(child, type_child))
          return false;

        child = ast_sibling(child);
        type_child = ast_sibling(type_child);
      }

      assert(type_child == NULL);
      return true;
    }

    case TK_SEQ:
    {
      // Occurs when there is a tuple on the left. Each child of the tuple will
      // be a sequence, but only sequences with a single writeable child are
      // valid. Other types won't appear here.
      return safe_to_write(ast_child(ast), type);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool safe_to_autorecover(ast_t* receiver_type, ast_t* type)
{
  switch(ast_id(receiver_type))
  {
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(receiver_type);

      while(child != NULL)
      {
        if(safe_to_autorecover(child, type))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(receiver_type);

      while(child != NULL)
      {
        if(!safe_to_autorecover(child, type))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      // An argument or result is safe for autorecover if it would be safe to
      // write into the receiver.
      return safe_field_write(cap_single(receiver_type), type);
    }

    case TK_ARROW:
    {
      // If the receiver is an arrow type, it's safe to autorecover if the
      // type being considered is safe to write into the upper bounds of the
      // receiver.
      ast_t* upper = viewpoint_upper(receiver_type);

      if(upper == NULL)
        return false;

      bool ok = safe_to_autorecover(upper, type);

      if(upper != receiver_type)
        ast_free_unattached(upper);

      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}
