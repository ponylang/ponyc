#include "control.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

bool expr_seq(ast_t* ast)
{
  // We might already have a type due to a return expression.
  ast_t* type = ast_type(ast);
  ast_t* last = ast_childlast(ast);
  ast_t* last_type = ast_type(last);

  // Type is unioned with the type of the last child.
  type = type_union(type, last_type);

  ast_settype(ast, type);
  ast_inheriterror(ast);

  // TODO: if we are the body of try expression, we need to push
  // our symbol consumes to the else and then blocks
  // In the body of a try, we might stop execution with any node that can
  // error out, so a DEFINE can only override an UNDEFINE *for the else block*
  // if it appears before the next node that can error out.
  // mark undefined as undefined_throw when we hit a throw
  // when we mark undefined_throw as defined, mark it as defined_throw instead
  // when we mark defined_throw as undefined, mark it as undefined_throw instead
  // treat defined_throw and undefined_throw like defined and undefined
  // but when we push our symbol status to the else block, treat both as
  // undefined
  // only need to do this in a try body
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

  if(type == NULL)
  {
    if(ast_sibling(ast) != NULL)
    {
      ast_error(ast_sibling(ast), "unreachable code");
      return false;
    }
  } else {
    if(l_type == NULL)
    {
      // Left side returns, get symbol status from the right.
      ast_inheritstatus(ast, right);
    } else if(r_type == NULL) {
      // Right side returns, get symbol status from the left.
      ast_inheritstatus(ast, left);
    } else {
      // Defined if defined in both branches. Undefined if undefined in either
      // branch.
      ast_inheritbranch(ast, left);
      ast_inheritbranch(ast, right);
      ast_consolidate_branches(ast, 2);
    }
  }

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_while(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  ast_t* cond_type = ast_type(cond);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  if(l_type == NULL)
  {
    ast_error(ast, "loop body can never repeat");
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = type_union(l_type, r_type);
  type = type_union(type, ast_type(ast));

  // TODO: A break statement in the body means some definitions might not
  // happen.
  if(r_type == NULL)
  {
    // Right side returns, get symbol status from the left.
    ast_inheritstatus(ast, left);
  } else {
    // Defined if defined in both branches. Undefined if undefined in either
    // branch.
    ast_inheritbranch(ast, left);
    ast_inheritbranch(ast, right);
    ast_consolidate_branches(ast, 2);
  }

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_repeat(ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* body_type = ast_type(body);
  ast_t* cond_type = ast_type(cond);
  ast_t* else_type = ast_type(else_clause);

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  if(body_type == NULL)
  {
    ast_error(ast, "loop body can never repeat");
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = type_union(body_type, else_type);
  type = type_union(type, ast_type(body));

  // TODO: A break statement in the body means some definitions might not
  // happen.
  if(else_type == NULL)
  {
    // Else clause returns, get symbol status from the body.
    ast_inheritstatus(ast, body);
  } else {
    // Defined if defined in both branches. Undefined if undefined in either
    // branch.
    ast_inheritbranch(ast, body);
    ast_inheritbranch(ast, else_clause);
    ast_consolidate_branches(ast, 2);
  }

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_try(ast_t* ast)
{
  // TODO: init tracking
  // propagate consumes, but not defines to the else clause
  // Override with 'then' branch, because it always executes.
  // Then push our settings to our parent.
  ast_t* body = ast_child(ast);
  ast_t* else_clause = ast_sibling(body);
  ast_t* then_clause = ast_sibling(else_clause);

  // it has to be possible for the left side to result in an error
  if(!ast_canerror(body))
  {
    ast_error(body, "try expression never results in an error");
    return false;
  }

  // The then clause does not affect the type of the expression.
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);
  ast_t* then_type = ast_type(then_clause);
  ast_t* type = type_union(body_type, else_type);

  if((type == NULL) && (ast_sibling(ast) != NULL))
  {
    ast_error(ast_sibling(ast), "unreachable code");
    return false;
  }

  if(then_type == NULL)
  {
    ast_error(then_clause, "then clause always terminates the function");
    return false;
  }

  ast_settype(ast, type);

  // Doesn't inherit error from the body.
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

  // Has no type.
  ast_inheriterror(ast);

  // Add type to loop.
  ast_t* loop_type = ast_type(loop);
  type = type_union(type, loop_type);
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

  // Has no type.
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

  if(ast_id(fun) == TK_NEW)
  {
    ast_error(ast, "can't return a value in a constructor");
    return false;
  }

  if(ast_id(fun) == TK_BE)
  {
    ast_error(ast, "can't return a value in a behaviour");
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

  // Has no type.
  ast_inheriterror(ast);

  // Add an additional type to the function body.
  ast_t* fun_body = ast_childidx(fun, 6);
  ast_t* fun_type = ast_type(fun_body);
  type = type_union(type, fun_type);
  ast_settype(fun_body, type);

  return true;
}
