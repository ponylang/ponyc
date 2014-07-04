#include "control.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

bool expr_seq(ast_t* ast)
{
  // type is the type of the last child
  ast_t* last = ast_childlast(ast);
  ast_settype(ast, ast_type(last));
  ast_inheriterror(ast);
  return true;
}

bool expr_if(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  if(type_bool(cond) == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type;

  if(ast_id(right) == TK_NONE)
    r_type = nominal_builtin(ast, "None");
  else
    r_type = ast_type(right);

  ast_t* type = type_union(ast, l_type, r_type);
  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_while(ast_t* ast)
{
  ast_t* cond = ast_child(ast);

  if(type_bool(cond) == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_settype(ast, nominal_builtin(ast, "None"));
  ast_inheriterror(ast);
  return true;
}

bool expr_repeat(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* cond = ast_sibling(body);

  if(type_bool(cond) == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_settype(ast, nominal_builtin(ast, "None"));
  ast_inheriterror(ast);
  return true;
}

bool expr_try(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  // it has to be possible for the left side to result in an error
  if(!ast_canerror(left))
  {
    ast_error(left, "try expression never results in an error");
    return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type;

  if(ast_id(right) == TK_NONE)
    r_type = nominal_builtin(ast, "None");
  else
    r_type = ast_type(right);

  ast_t* type = type_union(ast, l_type, r_type);
  ast_settype(ast, type);

  if(ast_canerror(right))
    ast_seterror(ast);

  return true;
}

bool expr_continue(ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);

  if(loop == NULL)
  {
    ast_error(ast, "must be in a loop");
    return false;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "is followed with this expression");
    return false;
  }

  return true;
}

bool expr_return(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* type = ast_type(body);
  ast_t* fun = ast_enclosing_method_body(ast);
//  bool ok = true;

  if(fun == NULL)
  {
    ast_error(ast, "return must occur in a function or a behaviour body");
    return false;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "is followed with this expression");
//    ok = false;
  }

  switch(ast_id(fun))
  {
    case TK_NEW:
      // TODO: too strict?
      ast_error(ast, "cannot return in a constructor");
      return false;

    case TK_BE:
    case TK_FUN:
    {
      ast_t* result = ast_childidx(fun, 4);

      if(!is_subtype(ast, type, result))
      {
        ast_error(body,
          "body of return doesn't match the function return type");
        return false;
      }

      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}
