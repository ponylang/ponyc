#include "finalisers.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef enum
{
  FINAL_NO_SEND = 0,
  FINAL_CAN_SEND = (1 << 0),
  FINAL_RECURSE = (1 << 1)
} final_send_t;

static final_send_t check_body_send(ast_t* ast);

static void show_send(ast_t* ast)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    if(ast_cansend(child) || ast_mightsend(child))
      show_send(child);

    child = ast_sibling(child);
  }

  if(ast_id(ast) == TK_CALL)
  {
    if(ast_cansend(ast))
      ast_error(ast, "a message can be sent here");

    if(ast_mightsend(ast))
      ast_error(ast, "a message might be sent here");
  }
}

static final_send_t check_call_send(ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, lhs);
  AST_GET_CHILDREN(lhs, receiver, method);
  ast_t* typeargs = NULL;

  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_FUNREF:
      // Qualified. Get the real receiver.
      typeargs = receiver;
      receiver = ast_child(receiver);
      method = ast_sibling(receiver);
      break;

    default: {}
  }

  ast_t* type = ast_type(receiver);

  // If we don't know the final type, we can't be certain of what all
  // implementations of the method do. Leave it as might send.
  if(!is_known(type))
    return FINAL_CAN_SEND;

  // TODO: if it's an intersection type, we need the method on the concrete
  // type. we need to set flags on the real one, not this reified one.
  ast_t* find = lookup(NULL, receiver, type, ast_name(method));
  assert(find != NULL);

  AST_GET_CHILDREN(find, cap, id, typeparams, params, result, can_error, body);

  final_send_t r = check_body_send(body);

  if(r == FINAL_NO_SEND)
  {
    // Mark the call as no send.
    ast_clearmightsend(ast);
  } else if((r & FINAL_CAN_SEND) != 0) {
    // Mark the call as can send.
    ast_setsend(ast);
  }

  return r;
}

static final_send_t check_expr_send(ast_t* ast)
{
  final_send_t send = FINAL_NO_SEND;

  if(ast_id(ast) == TK_CALL)
    send |= check_call_send(ast);

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    if(ast_mightsend(child))
      send |= check_expr_send(child);

    child = ast_sibling(child);
  }

  return send;
}

static final_send_t check_body_send(ast_t* ast)
{
  if(ast_inprogress(ast))
    return FINAL_RECURSE;

  if(ast_cansend(ast))
    return FINAL_CAN_SEND;

  if(!ast_mightsend(ast))
    return FINAL_NO_SEND;

  ast_setinprogress(ast);

  final_send_t r = check_expr_send(ast);

  if(r == FINAL_NO_SEND)
  {
    // Mark the body as no send.
    ast_clearmightsend(ast);
  } else if((r & FINAL_CAN_SEND) != 0) {
    // Mark the body as can send.
    ast_setsend(ast);
  }

  ast_clearinprogress(ast);
  return r;
}

static bool entity_finaliser(ast_t* entity, const char* final)
{
  ast_t* ast = ast_get(entity, final, NULL);

  if(ast == NULL)
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);
  final_send_t r = check_body_send(body);

  if((r & FINAL_CAN_SEND) != 0)
  {
    ast_error(ast, "_final cannot create actors or send messages");
    show_send(body);
    return false;
  }

  return true;
}

static bool module_finalisers(ast_t* module, const char* final)
{
  ast_t* entity = ast_child(module);
  bool ok = true;

  while(entity != NULL)
  {
    switch(ast_id(entity))
    {
      case TK_ACTOR:
      case TK_CLASS:
      case TK_PRIMITIVE:
        if(!entity_finaliser(entity, final))
          ok = false;
        break;

      default: {}
    }

    entity = ast_sibling(entity);
  }

  return ok;
}

static bool package_finalisers(ast_t* package, const char* final)
{
  ast_t* module = ast_child(package);
  bool ok = true;

  while(module != NULL)
  {
    if(!module_finalisers(module, final))
      ok = false;

    module = ast_sibling(module);
  }

  return ok;
}

bool pass_finalisers(ast_t* program)
{
  // TODO: remove this
  return true;

  ast_t* package = ast_child(program);
  const char* final = stringtab("_final");
  bool ok = true;

  while(package != NULL)
  {
    if(!package_finalisers(package, final))
      ok = false;

    package = ast_sibling(package);
  }

  return ok;
}
