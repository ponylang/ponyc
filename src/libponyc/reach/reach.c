#include "reach.h"
#include "../codegen/genname.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/reify.h"
#include <assert.h>

static void reach_call(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_BEREF:
    case TK_FUNREF:
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  ast_t* type = ast_type(receiver);
  const char* method_name = ast_name(method);
  reach_method(c, type, method_name, typeargs);
}

static void reach_expr(compile_t* c, ast_t* ast)
{
  // If this is a method call, mark the method as reachable.
  if(ast_id(ast) == TK_CALL)
    reach_call(c, ast);

  // TODO: look here for pattern matching on type when looking for interfaces
  // could build a set of fully reified interfaces that get pattern matched
  // later, we could go through all our reified types, and if they are that
  // inteface, we can add the interface to the provides list

  // Traverse all child expressions looking for calls.
  ast_t* child = ast_child(ast);

  while(child != NULL)
    reach_expr(c, child);
}

static void reach_body(compile_t* c, ast_t* fun)
{
  // TODO: trace the body, how do we make sure we haven't already?
  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error, body);
  reach_expr(c, body);
}

void reach_method(compile_t* c, ast_t* type, const char* name, ast_t* typeargs)
{
  // Get a reified method for this type.
  ast_t* this_type = set_cap_and_ephemeral(type, TK_REF, TK_NONE);
  ast_t* fun = lookup(NULL, NULL, this_type, name);
  ast_free_unattached(this_type);
  assert(fun != NULL);

  if(typeargs != NULL)
  {
    // Reify the function with its own typeargs, if it has any.
    AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error,
      body);

    ast_t* r_fun = reify(fun, typeparams, typeargs);
    ast_free_unattached(fun);
    fun = r_fun;
  }

  // Store reachability information for the method.
  const char* type_name = genname_type(type);
  const char* method_name = genname_fun(type_name, name, typeargs);

  if(ast_id(fun) == TK_NEW)
  {
    // TODO: store type_name -> name -> typeargs
  } else {
    // TODO: store name -> typeargs
  }

  // TODO: store method_name -> colour
  (void)method_name;

  // Traverse the body of the method looking for other reachable methods.
  reach_body(c, fun);
  ast_free_unattached(fun);
}
