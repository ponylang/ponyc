#include "control.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

bool expr_seq(ast_t* ast)
{
  // we might already have a type due to return expressions
  ast_t* type = ast_type(ast);
  ast_t* last = ast_childlast(ast);

  // type is unioned with the type of the last child
  ast_settype(ast, type_union(ast, type, ast_type(last)));
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

  // union with any existing type due to a break expression
  ast_t* type = type_union(ast, l_type, r_type);
  type = type_union(ast, type, ast_type(ast));

  ast_settype(ast, type);
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

  // union with any existing type due to a continue expression
  ast_t* type = ast_type(ast);
  type = type_union(ast, type, ast_type(body));
  ast_settype(ast, type);
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

bool expr_break(ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);
  ast_t* body = ast_child(ast);
  ast_t* type = ast_type(body);

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

  // add type to loop
  ast_t* loop_type = ast_type(loop);
  ast_settype(loop, type_union(ast, type, loop_type));
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

  // nothing in while loop, add None to repeat loop
  if(ast_id(loop) == TK_REPEAT)
  {
    ast_t* loop_type = ast_type(loop);
    ast_t* none = nominal_builtin(ast, "None");
    ast_settype(loop, type_union(ast, none, loop_type));
  }

  return true;
}

bool expr_return(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* type = ast_type(body);
  ast_t* fun = ast_enclosing_method_body(ast);

  if(fun == NULL)
  {
    ast_error(ast, "return must occur in a method body");
    return false;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "is followed with this expression");
    return false;
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

      // add an additional type to the function body
      ast_t* fun_body = ast_childidx(fun, 6);
      ast_t* fun_type = ast_type(fun_body);
      ast_settype(fun_body, type_union(ast, type, fun_type));
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}
