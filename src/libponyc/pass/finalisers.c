#include "finalisers.h"
#include <string.h>

static bool show_can_send(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  bool found = false;

  while(child != NULL)
  {
    if(ast_cansend(child) || ast_mightsend(child))
      found |= show_can_send(child);

    child = ast_sibling(child);
  }

  if(found)
    return true;

  if(ast_cansend(ast))
  {
    ast_error(ast, "a message can be sent here");
    return true;
  }

  if(ast_mightsend(ast))
  {
    ast_error(ast, "a message might be sent here");
    return true;
  }

  return false;
}

static void check_might_send(ast_t* ast)
{
  AST_GET_CHILDREN(ast, receiver, method);
  ast_t* typeargs = NULL;

  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_FUNREF:
      // Qualified. Get the real receiver.
      typeargs = receiver;
      receiver = ast_child(receiver);
      break;

    default: {}
  }

  // ast_t* type = ast_type(receiver);
  // ast_t* find = lookup(NULL, receiver, )

  int i = 0;
  i++;
}

static bool show_might_send(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  bool found = false;

  while(child != NULL)
  {
    switch(ast_id(child))
    {
      case TK_NEWREF:
      case TK_FUNREF:
      {
        check_might_send(child);
        break;
      }

      default: {}
    }

    if(ast_mightsend(child))
      found |= show_might_send(child);

    child = ast_sibling(child);
  }

  if(found)
    return true;

  if(ast_mightsend(ast))
  {
    ast_error(ast, "a message might be sent here");
    return true;
  }

  return false;
}
static bool entity_finaliser(ast_t* entity, const char* final)
{
  ast_t* ast = ast_get(entity, final, NULL);

  if(ast == NULL)
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  if(ast_cansend(body))
  {
    ast_error(ast, "_final cannot create actors or send messages");
    show_can_send(body);
    return false;
  }

  if(ast_mightsend(body))
  {
    ast_error(ast,
      "_final cannot call methods that might create actors or send messages");

    if(show_might_send(body))
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
