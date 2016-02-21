#include "finalisers.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

enum
{
  FINAL_NO_SEND = 0,
  FINAL_CAN_SEND = (1 << 0),
  FINAL_RECURSE = (1 << 1)
};

static int check_body_send(ast_t* ast, bool in_final);

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
    else if(ast_mightsend(ast))
      ast_error(ast, "a message might be sent here");
  }
}

static ast_t* receiver_def(ast_t* type)
{
  // We must be a known type at this point.
  switch(ast_id(type))
  {
    case TK_ISECTTYPE:
    {
      // Find the first concrete type in the intersection.
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* def = receiver_def(child);

        if(def != NULL)
        {
          switch(ast_id(def))
          {
            case TK_PRIMITIVE:
            case TK_STRUCT:
            case TK_CLASS:
            case TK_ACTOR:
              return def;

            default: {}
          }
        }

        child = ast_sibling(child);
      }

      break;
    }

    case TK_NOMINAL:
      // Return the def.
      return (ast_t*)ast_data(type);

    case TK_ARROW:
      // Use the right-hand side.
      return receiver_def(ast_childidx(type, 1));

    case TK_TYPEPARAMREF:
    {
      // Use the constraint.
      ast_t* def = (ast_t*)ast_data(type);
      return receiver_def(ast_childidx(def, 1));
    }

    default: {}
  }

  return NULL;
}

static int check_call_send(ast_t* ast, bool in_final)
{
  AST_GET_CHILDREN(ast, positional, named, lhs);
  AST_GET_CHILDREN(lhs, receiver, method);

  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_FUNREF:
      // Qualified. Get the real receiver.
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

  ast_t* def = receiver_def(type);
  assert(def != NULL);

  const char* method_name = ast_name(method);
  ast_t* fun = ast_get(def, method_name, NULL);
  assert(fun != NULL);

  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error, body);
  int r = check_body_send(body, false);

  if(r == FINAL_NO_SEND)
  {
    // Mark the call as no send.
    ast_clearmightsend(ast);
  } else if(in_final && (r == FINAL_RECURSE)) {
    // If we're in the finaliser, which can't recurse, we treat a recurse as
    // a no send.
    ast_clearmightsend(ast);
  } else if((r & FINAL_CAN_SEND) != 0) {
    // Mark the call as can send.
    ast_setsend(ast);
  }

  return r;
}

static int check_expr_send(ast_t* ast, bool in_final)
{
  int send = FINAL_NO_SEND;

  if(ast_id(ast) == TK_CALL)
    send |= check_call_send(ast, in_final);

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    if(ast_mightsend(child))
      send |= check_expr_send(child, in_final);

    child = ast_sibling(child);
  }

  return send;
}

static int check_body_send(ast_t* ast, bool in_final)
{
  if(ast_checkflag(ast, AST_FLAG_RECURSE_1))
    return FINAL_RECURSE;

  if(ast_cansend(ast))
    return FINAL_CAN_SEND;

  if(!ast_mightsend(ast))
    return FINAL_NO_SEND;

  ast_setflag(ast, AST_FLAG_RECURSE_1);

  int r = check_expr_send(ast, in_final);

  if(r == FINAL_NO_SEND)
  {
    // Mark the body as no send.
    ast_clearmightsend(ast);
  } else if((r & FINAL_CAN_SEND) != 0) {
    // Mark the body as can send.
    ast_setsend(ast);
  }

  ast_clearflag(ast, AST_FLAG_RECURSE_1);
  return r;
}

static bool entity_finaliser(ast_t* entity, const char* final)
{
  ast_t* ast = ast_get(entity, final, NULL);

  if(ast == NULL)
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);
  int r = check_body_send(body, true);

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
      case TK_STRUCT:
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
