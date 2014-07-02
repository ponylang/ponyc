#include "literal.h"
#include "../type/nominal.h"
#include "../type/cap.h"
#include <assert.h>

bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type = nominal_builtin(ast, name);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
  return true;
}

bool expr_this(ast_t* ast)
{
  ast_t* def = ast_enclosing_type(ast);
  assert(def != NULL);
  assert(ast_id(def) != TK_TYPE);

  ast_t* id = ast_child(def);
  ast_t* typeparams = ast_sibling(id);
  const char* name = ast_name(id);

  ast_t* nominal = ast_from(ast, TK_NOMINAL);
  ast_add(nominal, ast_from(ast, TK_NONE)); // ephemerality
  ast_add(nominal, ast_from(ast, cap_for_receiver(ast))); // capability

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    ast_t* typeparam = ast_child(typeparams);
    ast_t* typeargs = ast_from(ast, TK_TYPEARGS);
    ast_add(nominal, typeargs);

    while(typeparam != NULL)
    {
      ast_t* typeparam_id = ast_child(typeparam);
      ast_t* typearg = nominal_type(ast, NULL, ast_name(typeparam_id));
      ast_append(typeargs, typearg);

      typeparam = ast_sibling(typeparam);
    }
  } else {
    ast_add(nominal, ast_from(ast, TK_NONE)); // empty typeargs
  }

  ast_add(nominal, ast_from_string(ast, name));
  ast_add(nominal, ast_from(ast, TK_NONE));
  ast_settype(ast, nominal);

  return true;
}

bool expr_tuple(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type;

  if(ast_sibling(child) == NULL)
  {
    type = ast_type(child);
  } else {
    ast_t* tuple = ast_from(ast, TK_TUPLETYPE);
    type = tuple;

    ast_append(tuple, ast_type(child));
    child = ast_sibling(child);

    while(ast_sibling(child) != NULL)
    {
      ast_t* next = ast_from(ast, TK_TUPLETYPE);
      ast_append(tuple, next);
      tuple = next;

      ast_append(tuple, ast_type(child));
      child = ast_sibling(child);
    }

    ast_append(tuple, ast_type(child));
  }

  ast_settype(ast, type);
  return true;
}

bool expr_error(ast_t* ast)
{
  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "error must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "error is followed with this expression");
    return false;
  }

  ast_settype(ast, ast_from(ast, TK_ERROR));
  return true;
}
