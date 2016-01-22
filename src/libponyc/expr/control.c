#include "call.h"
#include "control.h"
#include "literal.h"
#include "../ast/astbuild.h"
#include "../ast/frame.h"
#include "../ast/token.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../pkg/ifdef.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include <assert.h>

bool expr_seq(pass_opt_t* opt, ast_t* ast)
{
  bool ok = true;

  // Any expression other than the last that is still literal is an error
  for(ast_t* p = ast_child(ast); ast_sibling(p) != NULL; p = ast_sibling(p))
  {
    ast_t* p_type = ast_type(p);

    if(is_typecheck_error(p_type))
    {
      ok = false;
    } else if(is_type_literal(p_type)) {
      ast_error(p, "Cannot infer type of unused literal");
      ok = false;
    }
  }

  // We might already have a type due to a return expression.
  ast_t* type = ast_type(ast);
  ast_t* last = ast_childlast(ast);

  if((type != NULL) && !coerce_literals(&last, type, opt))
    return false;

  // Type is unioned with the type of the last child.
  type = control_type_add_branch(type, last);
  ast_settype(ast, type);
  ast_inheritflags(ast);

  if(!ast_has_scope(ast))
    return ok;

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

  return ok;
}

// Determine which branch of the given ifdef to use and convert the ifdef into
// an if.
static bool resolve_ifdef(pass_opt_t* opt, ast_t* ast)
{
  assert(ast != NULL);

  // We turn the ifdef node into an if so that codegen doesn't need to know
  // about ifdefs at all.
  // We need to determine which branch to take. Note that since normalisation
  // adds the conditions of outer ifdefs (if any) it may be that BOTH our then
  // and else conditions fail. In this case we can pick either one, since an
  // outer ifdef will throw away the whole branch this ifdef is in anyway.
  AST_GET_CHILDREN(ast, cond, then_clause, else_clause, else_cond);
  bool then_value = ifdef_cond_eval(cond, opt);

  ast_setid(ast, TK_IF);
  REPLACE(&cond, NODE(TK_SEQ, NODE(then_value ? TK_TRUE : TK_FALSE)));
  // Don't need to set condition type since we've finished type checking it.

  // Don't need else condition any more.
  ast_remove(else_cond);
  return true;
}

bool expr_if(pass_opt_t* opt, ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  if(ast_id(ast) == TK_IF)
  {
    ast_t* cond_type = ast_type(cond);

    if(is_typecheck_error(cond_type))
      return false;

    if(!is_bool(cond_type))
    {
      ast_error(cond, "condition must be a Bool");
      return false;
    }
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(!is_control_type(l_type))
  {
    type = control_type_add_branch(type, left);
    ast_inheritbranch(ast, left);
    branch_count++;
  }

  if(!is_control_type(r_type))
  {
    type = control_type_add_branch(type, right);
    ast_inheritbranch(ast, right);
    branch_count++;
  }

  if(type == NULL)
  {
    if(ast_sibling(ast) != NULL)
    {
      ast_error(ast_sibling(ast), "unreachable code");
      return false;
    }

    type = ast_from(ast, TK_IF);
  }

  ast_settype(ast, type);
  ast_inheritflags(ast);
  ast_consolidate_branches(ast, branch_count);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  if(ast_id(ast) == TK_IFDEF)
    return resolve_ifdef(opt, ast);

  return true;
}

bool expr_while(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  ast_t* cond_type = ast_type(cond);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  if(is_typecheck_error(cond_type))
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  if(is_typecheck_error(body_type) || is_typecheck_error(else_type))
    return false;

  // All consumes have to be in scope when the loop body finishes.
  if(!ast_all_consumes_in_scope(body, body))
    return false;

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

  // No symbol status is inherited from the loop body. Nothing from outside the
  // loop body can be consumed, and definitions in the body may not occur.
  if(!is_control_type(body_type))
    type = control_type_add_branch(type, body);

  if(!is_control_type(else_type))
  {
    type = control_type_add_branch(type, else_clause);
    ast_inheritbranch(ast, body);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  if(type == NULL)
    type = ast_from(ast, TK_WHILE);

  ast_settype(ast, type);
  ast_inheritflags(ast);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_repeat(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* body_type = ast_type(body);
  ast_t* cond_type = ast_type(cond);
  ast_t* else_type = ast_type(else_clause);

  if(is_typecheck_error(cond_type))
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  if(is_typecheck_error(body_type) || is_typecheck_error(else_type))
    return false;

  // All consumes have to be in scope when the loop body finishes.
  if(!ast_all_consumes_in_scope(body, body))
    return false;

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

  // No symbol status is inherited from the loop body or condition. Nothing
  // from outside can be consumed, and definitions inside may not occur.
  if(!is_control_type(body_type))
    type = control_type_add_branch(type, body);

  if(!is_control_type(else_type))
  {
    type = control_type_add_branch(type, else_clause);
    ast_inheritbranch(ast, else_clause);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  if(type == NULL)
    type = ast_from(ast, TK_REPEAT);

  ast_settype(ast, type);
  ast_inheritflags(ast);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_try(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  // It has to be possible for the left side to result in an error.
  if((ast_id(ast) == TK_TRY) && !ast_canerror(body))
  {
    ast_error(body, "try expression never results in an error");
    return false;
  }

  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);
  ast_t* then_type = ast_type(then_clause);

  if(is_typecheck_error(body_type) ||
    is_typecheck_error(else_type) ||
    is_typecheck_error(then_type))
    return false;

  ast_t* type = NULL;

  if(!is_control_type(body_type))
    type = control_type_add_branch(type, body);

  if(!is_control_type(else_type))
    type = control_type_add_branch(type, else_clause);

  if(type == NULL)
  {
    if(ast_sibling(ast) != NULL)
    {
      ast_error(ast_sibling(ast), "unreachable code");
      return false;
    }

    type = ast_from(ast, TK_TRY);
  }

  // The then clause does not affect the type of the expression.
  if(is_control_type(then_type))
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

  if(ast_cansend(body) || ast_cansend(else_clause) || ast_cansend(then_clause))
    ast_setsend(ast);

  if(ast_mightsend(body) || ast_mightsend(else_clause) ||
    ast_mightsend(then_clause))
    ast_setmightsend(ast);

  literal_unify_control(ast, opt);

  // Push the symbol status from the then clause to our parent scope.
  ast_inheritstatus(ast_parent(ast), then_clause);
  return true;
}

bool expr_recover(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, expr);
  ast_t* type = ast_type(expr);

  if(is_typecheck_error(type))
    return false;

  ast_t* r_type = recover_type(type, ast_id(cap));

  if(r_type == NULL)
  {
    ast_error(ast, "can't recover to this capability");
    ast_error(expr, "expression type is %s", ast_print_type(type));
    return false;
  }

  ast_settype(ast, r_type);
  ast_inheritflags(ast);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), expr);
  return true;
}

bool expr_break(typecheck_t* t, ast_t* ast)
{
  if(t->frame->loop_body == NULL)
  {
    ast_error(ast, "must be in a loop");
    return false;
  }

  if(!ast_all_consumes_in_scope(t->frame->loop_body, ast))
    return false;

  // break is always the last expression in a sequence
  assert(ast_sibling(ast) == NULL);

  ast_settype(ast, ast_from(ast, TK_BREAK));
  ast_inheritflags(ast);

  // Add type to loop.
  ast_t* body = ast_child(ast);

  if(is_control_type(ast_type(body)))
  {
    ast_error(body, "break value cannot be a control statement");
    return false;
  }

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

  if(!ast_all_consumes_in_scope(t->frame->loop_body, ast))
    return false;

  // continue is always the last expression in a sequence
  assert(ast_sibling(ast) == NULL);

  ast_settype(ast, ast_from(ast, TK_CONTINUE));
  return true;
}

bool expr_return(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  // return is always the last expression in a sequence
  assert(ast_sibling(ast) == NULL);

  if(ast_parent(ast) == t->frame->method_body)
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

  if(is_typecheck_error(body_type))
    return false;

  if(is_control_type(body_type))
  {
    ast_error(body, "return value cannot be a control statement");
    return false;
  }

  bool ok = true;

  switch(ast_id(t->frame->method))
  {
    case TK_NEW:
      if(is_this_incomplete(t, ast))
      {
        ast_error(ast,
          "all fields must be defined before constructor returns");
        ok = false;
      }
      break;

    case TK_BE:
      assert(is_none(body_type));
      break;

    default:
    {
      // The body type must be a subtype of the return type, and an alias of
      // the body type must be a subtype of an alias of the return type.
      ast_t* a_type = alias(type);
      ast_t* a_body_type = alias(body_type);

      if(!is_subtype(body_type, type, true) ||
        !is_subtype(a_body_type, a_type, true))
      {
        ast_t* last = ast_childlast(body);
        ast_error(last, "returned value isn't the return type");
        ast_error(type, "function return type: %s", ast_print_type(type));
        ast_error(body_type, "returned value type: %s",
          ast_print_type(body_type));
        ok = false;
      }

      ast_free_unattached(a_type);
      ast_free_unattached(a_body_type);
    }
  }

  ast_settype(ast, ast_from(ast, TK_RETURN));
  ast_inheritflags(ast);
  return ok;
}

bool expr_error(ast_t* ast)
{
  // error is always the last expression in a sequence
  assert(ast_sibling(ast) == NULL);

  ast_settype(ast, ast_from(ast, TK_ERROR));
  ast_seterror(ast);
  return true;
}

bool expr_compile_error(ast_t* ast)
{
  // compile_error is always the last expression in a sequence
  assert(ast_sibling(ast) == NULL);

  ast_settype(ast, ast_from(ast, TK_COMPILE_ERROR));
  return true;
}
