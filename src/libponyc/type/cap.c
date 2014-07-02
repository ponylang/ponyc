#include "cap.h"
#include <assert.h>

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

  token_id cap = ast_id(ast_child(method));

  if(cap == TK_NONE)
    return TK_REF;

  return cap;
}

token_id cap_for_fun(ast_t* fun)
{
  switch(ast_id(fun))
  {
    case TK_NEW:
    case TK_BE:
      return TK_TAG;

    case TK_FUN:
      return ast_id(ast_child(fun));

    default: {}
  }

  assert(0);
  return TK_NONE;
}
