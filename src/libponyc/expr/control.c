#include "control.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

bool expr_seq(ast_t* ast)
{
  // we might already have a type due to return expressions
  ast_t* type = ast_type(ast);
  ast_t* last = ast_childlast(ast);
  ast_t* last_type = ast_type(last);

  // type is unioned with the type of the last child
  if(type != NULL)
  {
    last_type = type_union(type, last_type);
    last_type = type_literal_to_runtime(last_type);
  }

  ast_settype(ast, last_type);
  ast_inheriterror(ast);
  return true;
}

bool expr_if(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  if(!is_bool(ast_type(cond)))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);
  ast_t* type = type_union(l_type, r_type);
  type = type_literal_to_runtime(type);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_while(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  if(!is_bool(ast_type(cond)))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  // union with any existing type due to a break expression
  ast_t* type = type_union(l_type, r_type);
  type = type_union(type, ast_type(ast));
  type = type_literal_to_runtime(type);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_repeat(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* cond = ast_sibling(body);

  if(!is_bool(ast_type(cond)))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  // union with any existing type due to a continue expression
  ast_t* type = ast_type(ast);
  type = type_union(type, ast_type(body));
  type = type_literal_to_runtime(type);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_try(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* else_clause = ast_sibling(body);
  ast_t* then_clause = ast_sibling(else_clause);

  // it has to be possible for the left side to result in an error
  if(!ast_canerror(body))
  {
    ast_error(body, "try expression never results in an error");
    return false;
  }

  // the then clause does not affect the type of the expression
  ast_t* l_type = ast_type(body);
  ast_t* r_type = ast_type(else_clause);
  ast_t* type = type_union(l_type, r_type);
  type = type_literal_to_runtime(type);

  ast_settype(ast, type);

  // doesn't inherit error from the body
  if(ast_canerror(else_clause) || ast_canerror(then_clause))
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
  type = type_union(type, loop_type);
  type = type_literal_to_runtime(type);
  ast_settype(loop, type);
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
    ast_t* none = type_builtin(ast, "None");
    ast_settype(loop, type_union(none, loop_type));
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

  ast_t* result = ast_childidx(fun, 4);

  if(!is_subtype(type, result))
  {
    ast_error(body,
      "body of return doesn't match the function return type");
    return false;
  }

  // add an additional type to the function body
  ast_t* fun_body = ast_childidx(fun, 6);
  ast_t* fun_type = ast_type(fun_body);
  type = type_union(type, fun_type);
  type = type_literal_to_runtime(type);

  ast_settype(fun_body, type);
  return true;
}
