#include "cap.h"
#include <assert.h>

static token_id cap_upper_bounds(token_id a, token_id b)
{
  assert((a >= TK_ISO) && (a <= TK_TAG));
  assert((b >= TK_ISO) && (b <= TK_TAG));

  if(is_cap_sub_cap(a, b))
    return b;

  if(is_cap_sub_cap(b, a))
    return a;

  // one is ref and the other is val
  return TK_BOX;
}

static token_id cap_lower_bounds(token_id a, token_id b)
{
  assert((a >= TK_ISO) && (a <= TK_TAG));
  assert((b >= TK_ISO) && (b <= TK_TAG));

  if(is_cap_sub_cap(a, b))
    return a;

  if(is_cap_sub_cap(b, a))
    return b;

  // one is ref and the other is val
  assert(0);
  return TK_NONE;
}

bool is_cap_sub_cap(token_id sub, token_id super)
{
  assert((sub >= TK_ISO) && (sub <= TK_TAG));
  assert((super >= TK_ISO) && (super <= TK_TAG));

  if((sub == TK_REF) && (super == TK_VAL))
    return false;

  return sub <= super;
}

token_id cap_for_receiver(ast_t* ast)
{
  ast_t* method = ast_enclosing_method(ast);

  // if we aren't in a method, we're in a field initialiser
  if(method == NULL)
    return TK_REF;

  // if it's a function, get the capability from the signature
  if(ast_id(method) == TK_FUN)
    return ast_id(ast_child(method));

  // if it's a behaviour or a constructor, it's a ref
  return TK_REF;
}

token_id cap_for_fun(ast_t* fun)
{
  assert((ast_id(fun) == TK_NEW) ||
    (ast_id(fun) == TK_BE) ||
    (ast_id(fun) == TK_FUN)
    );

  return ast_id(ast_child(fun));
}

token_id cap_for_type(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      return cap_upper_bounds(cap_for_type(left), cap_for_type(right));
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(type);
      ast_t* right = ast_sibling(left);
      return cap_lower_bounds(cap_for_type(left), cap_for_type(right));
    }

    case TK_TUPLETYPE:
    {
      // should never be looking for the cap of a tuple
      break;
    }

    case TK_NOMINAL:
      return ast_id(ast_childidx(type, 3));

    case TK_STRUCTURAL:
      return ast_id(ast_childidx(type, 1));

    case TK_ARROW:
    {
      // TODO: viewpoint adaptation
      return cap_for_type(ast_childidx(type, 1));
    }

    default: {}
  }

  assert(0);
  return TK_NONE;
}
