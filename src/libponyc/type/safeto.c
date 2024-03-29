#include "safeto.h"
#include "cap.h"
#include "viewpoint.h"
#include "ponyassert.h"

static bool safe_field_move(token_id cap, ast_t* type, direction direction)
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
        if(!safe_field_move(cap, child, direction))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      // Safe to write if the lower bounds is safe to write.
      ast_t* upper = viewpoint_lower(type);

      if(upper == NULL)
        return false;

      bool ok = safe_field_move(cap, upper, direction);

      if(upper != type)
        ast_free_unattached(upper);

      return ok;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return cap_safetomove(cap, cap_single(type), direction);

    default: {}
  }

  pony_assert(0);
  return false;
}

bool safe_to_move(ast_t* ast, ast_t* type, direction direction)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_VARREF:
    case TK_DONTCARE:
    case TK_DONTCAREREF:
      return true;

    case TK_TUPLEELEMREF:
      return true; // this isn't actually allowed, but it will be caught later.

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
      if(safe_field_move(l_cap, type, direction))
        return true;

      // If the field type (without adaptation) is safe, then it's ok as
      // well. So iso.tag = ref should be allowed.
      ast_t* r_type = ast_type(right);
      return safe_field_move(l_cap, r_type, direction);
    }

    case TK_TUPLE:
    {
      // At this point, we know these will be the same length.
      pony_assert(ast_id(type) == TK_TUPLETYPE);
      ast_t* child = ast_child(ast);
      ast_t* type_child = ast_child(type);

      while(child != NULL)
      {
        if(!safe_to_move(child, type_child, direction))
          return false;

        child = ast_sibling(child);
        type_child = ast_sibling(type_child);
      }

      pony_assert(type_child == NULL);
      return true;
    }

    case TK_SEQ:
    {
      // Occurs when there is a tuple on the left. Each child of the tuple will
      // be a sequence, but only sequences with a single writeable child are
      // valid. Other types won't appear here.
      return safe_to_move(ast_child(ast), type, direction);
    }

    default: {}
  }

  pony_assert(0);
  return false;
}

bool safe_to_autorecover(ast_t* receiver_type, ast_t* type, direction direction)
{
  switch(ast_id(receiver_type))
  {
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(receiver_type);

      while(child != NULL)
      {
        if(safe_to_autorecover(child, type, direction))
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
        if(!safe_to_autorecover(child, type, direction))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      // An argument or result is safe for autorecover if it would be safe to
      // move into/out of the receiver, respectively.
      return safe_field_move(cap_single(receiver_type), type, direction);
    }

    case TK_ARROW:
    {
      // If the receiver is an arrow type, it's safe to autorecover if the
      // type being considered is safe to move into/out of the upper bounds of the
      // receiver.
      ast_t* upper = viewpoint_upper(receiver_type);

      if(upper == NULL)
        return false;

      bool ok = safe_to_autorecover(upper, type, direction);

      if(upper != receiver_type)
        ast_free_unattached(upper);

      return ok;
    }

    default: {}
  }

  pony_assert(0);
  return false;
}

bool safe_to_mutate(ast_t* ast)
{
  switch (ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_VARREF:
    case TK_DONTCARE:
    case TK_DONTCAREREF:
      return true;

    case TK_TUPLEELEMREF:
      return true; // this isn't actually allowed, but it will be caught later.

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
      return cap_mutable(l_cap);
    }

    case TK_TUPLE:
    {
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!safe_to_mutate(child))
          return false;

        child = ast_sibling(child);
      }
      return true;
    }

    case TK_SEQ:
    {
      // Occurs when there is a tuple on the left. Each child of the tuple will
      // be a sequence, but only sequences with a single writeable child are
      // valid. Other types won't appear here.
      return safe_to_mutate(ast_child(ast));
    }

    default: {}
  }

  pony_assert(0);
  return false;
}
