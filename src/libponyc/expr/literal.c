#include "literal.h"
#include "../type/assemble.h"
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
  ast_settype(ast, type_for_this(ast, cap_for_receiver(ast), false));
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
