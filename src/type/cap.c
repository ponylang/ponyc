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
  assert(method != NULL);

  return ast_id(ast_child(method));
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

ast_t* cap_from_rawcap(ast_t* ast, token_id rawcap)
{
  ast_t* cap = ast_from(ast, TK_CAP);
  ast_t* rcap = ast_from(ast, rawcap);
  ast_add(cap, rcap);

  return cap;
}
