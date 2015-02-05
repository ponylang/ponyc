#include "control.h"
#include "literal.h"
#include "../ast/frame.h"
#include "../ast/token.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include <assert.h>

bool expr_seq(ast_t* ast)
{
  // Any expression other than the last that is still literal is an error
  for(ast_t* p = ast_child(ast); ast_sibling(p) != NULL; p = ast_sibling(p))
  {
    ast_t* p_type = ast_type(p);

    if(p_type == NULL)
      return false;

    if(is_type_literal(p_type))
    {
      ast_error(p, "Cannot infer type of unused literal");
      return false;
    }
  }

  // We might already have a type due to a return expression.
  ast_t* type = ast_type(ast);
  ast_t* last = ast_childlast(ast);

  // Type is unioned with the type of the last child.
  type = control_type_add_branch(type, last);
  ast_settype(ast, type);
  ast_inheriterror(ast);

  if(!ast_has_scope(ast))
    return true;

  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_TRY:
    case TK_TRY_NO_CHECK:
    {
      // Propagate consumes forward in a try expression.
      AST_GET_CHILDREN(parent, body, else_clause, then_clause);

      if(body == ast)
      {
        // Push our consumes, but not defines, to the else clause.
        ast_inheritbranch(else_clause, body);
        ast_consolidate_branches(else_clause, 2);
      } else if(else_clause == ast) {
        // Push our consumes, but not defines, to the then clause. This
        // includes the consumes from the body.
        ast_inheritbranch(then_clause, else_clause);
        ast_consolidate_branches(then_clause, 2);
      }
    }

    default: {}
  }

  return true;
}

bool expr_if(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);
  ast_t* cond_type = ast_type(cond);

  if(cond_type == NULL)
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(l_type != NULL)
  {
    type = control_type_add_branch(type, left);
    ast_inheritbranch(ast, left);
    branch_count++;
  }

  if(r_type != NULL)
  {
    type = control_type_add_branch(type, right);
    ast_inheritbranch(ast, right);
    branch_count++;
  }

  if((type == NULL) && (ast_sibling(ast) != NULL))
  {
    ast_error(ast_sibling(ast), "unreachable code");
    return false;
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  ast_consolidate_branches(ast, branch_count);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_while(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  ast_t* cond_type = ast_type(cond);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  if(cond_type == NULL)
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  if(body_type == NULL)
  {
    // All body code paths end with return, break, etc
    ast_error(ast, "loop body can never repeat");
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

  // No symbol status is inherited from the loop body. Nothing from outside the
  // loop body can be consumed, and definitions in the body may not occur.
  type = control_type_add_branch(type, body);

  if(else_type != NULL)
  {
    type = control_type_add_branch(type, else_clause);
    ast_inheritbranch(ast, body);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_repeat(ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* body_type = ast_type(body);
  ast_t* cond_type = ast_type(cond);
  ast_t* else_type = ast_type(else_clause);

  if(cond_type == NULL)
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  if(body_type == NULL)
  {
    // All body code paths end with return, break, etc
    ast_error(ast, "loop body can never repeat");
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

  // No symbol status is inherited from the loop body or condition. Nothing
  // from outside can be consumed, and definitions inside may not occur.
  type = control_type_add_branch(type, body);

  if(else_type != NULL)
  {
    type = control_type_add_branch(type, else_clause);
    ast_inheritbranch(ast, else_clause);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_try(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* else_clause = ast_sibling(body);
  ast_t* then_clause = ast_sibling(else_clause);

  // It has to be possible for the left side to result in an error.
  if((ast_id(ast) == TK_TRY) && !ast_canerror(body))
  {
    ast_error(body, "try expression never results in an error");
    return false;
  }

  ast_t* type = control_type_add_branch(NULL, body);
  type = control_type_add_branch(type, else_clause);

  if((type == NULL) && (ast_sibling(ast) != NULL))
  {
    ast_error(ast_sibling(ast), "unreachable code");
    return false;
  }

  // The then clause does not affect the type of the expression.
  ast_t* then_type = ast_type(then_clause);

  if(then_type == NULL)
  {
    ast_error(then_clause, "then clause always terminates the function");
    return false;
  }

  if(is_type_literal(then_type))
  {
    ast_error(then_clause, "Cannot infer type of unused literal");
    return false;
  }

  ast_settype(ast, type);

  // Doesn't inherit error from the body.
  if(ast_canerror(else_clause) || ast_canerror(then_clause))
    ast_seterror(ast);

  // Push the symbol status from the then clause to our parent scope.
  ast_inheritstatus(ast_parent(ast), then_clause);
  return true;
}

bool expr_recover(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, expr);
  ast_t* type = ast_type(expr);

  if(type == NULL)
    return false;

  ast_t* r_type = recover_type(type, ast_id(cap));

  if(r_type == NULL)
  {
    ast_error(ast, "can't recover to this capability");
    ast_error(expr, "expression type is %s", ast_print_type(type));
    return false;
  }

  ast_settype(ast, r_type);
  ast_inheriterror(ast);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_break(typecheck_t* t, ast_t* ast)
{
  if(t->frame->loop_body == NULL)
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
  ast_t* body = ast_child(ast);
  ast_t* loop_type = ast_type(t->frame->loop);

  loop_type = control_type_add_branch(loop_type, body);
  ast_settype(t->frame->loop, loop_type);

  return true;
}

bool expr_continue(typecheck_t* t, ast_t* ast)
{
  if(t->frame->loop_body == NULL)
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

bool expr_return(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  if(t->frame->method_body == NULL)
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

  if(is_method_result(t, ast))
  {
    ast_error(ast,
      "use return only to exit early from a method, not at the end");
    return false;
  }

  ast_t* type = ast_childidx(t->frame->method, 4);
  ast_t* body = ast_child(ast);

  if(!coerce_literals(&body, type, opt))
    return false;

  ast_t* body_type = ast_type(body);

  if(body_type == NULL)
    return false;

  switch(ast_id(t->frame->method))
  {
    case TK_NEW:
      ast_error(ast, "can't return a value in a constructor");
      return false;

    case TK_BE:
      if(!is_none(body_type))
      {
        ast_error(ast, "return in a behaviour must return None");
        return false;
      }
      return true;

    default: {}
  }

  // The body type must be a subtype of the return type, and an alias of the
  // body type must be a subtype of an alias of the return type.
  ast_t* a_type = alias(type);
  ast_t* a_body_type = alias(body_type);
  bool ok = true;

  if(!is_subtype(body_type, type) || !is_subtype(a_body_type, a_type))
  {
    ast_t* last = ast_childlast(body);
    ast_error(last, "returned value isn't the return type");
    ast_error(type, "function return type: %s", ast_print_type(type));
    ast_error(body_type, "returned value type: %s", ast_print_type(body_type));
    ok = false;
  }

  ast_free_unattached(a_type);
  ast_free_unattached(a_body_type);

  // Has no type.
  ast_inheriterror(ast);
  return ok;
}

bool expr_error(ast_t* ast)
{
  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "error must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "error is followed with this expression");
    return false;
  }

  ast_seterror(ast);
  return true;
}
