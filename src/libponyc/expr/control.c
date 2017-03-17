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
#include "ponyassert.h"

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
      ast_error(opt->check.errors, p, "Cannot infer type of unused literal");
      ok = false;
    }
  }

  if(ok)
  {
    // We might already have a type due to a return expression.
    ast_t* type = ast_type(ast);
    ast_t* last = ast_childlast(ast);

    if((type != NULL) && !coerce_literals(&last, type, opt))
      return false;

    // Type is unioned with the type of the last child.
    type = control_type_add_branch(opt, type, last);
    ast_settype(ast, type);

    // If last expression jumps away with no value, then we do too.
    if(ast_checkflag(last, AST_FLAG_JUMPS_AWAY))
      ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  }

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
  pony_assert(ast != NULL);

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
      ast_error(opt->check.errors, cond, "condition must be a Bool");
      return false;
    }
  }

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(!ast_checkflag(left, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(left)))
      return false;

    type = control_type_add_branch(opt, type, left);
    ast_inheritbranch(ast, left);
    branch_count++;
  }

  if(!ast_checkflag(right, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(right)))
      return false;

    type = control_type_add_branch(opt, type, right);
    ast_inheritbranch(ast, right);
    branch_count++;
  }

  if(type == NULL)
  {
    if((ast_id(ast_parent(ast)) == TK_SEQ) && ast_sibling(ast) != NULL)
    {
      ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
      return false;
    }

    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  }

  ast_settype(ast, type);
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

  if(is_typecheck_error(cond_type))
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(opt->check.errors, cond, "condition must be a Bool");
    return false;
  }

  // All consumes have to be in scope when the loop body finishes.
  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(body, body, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

  // No symbol status is inherited from the loop body. Nothing from outside the
  // loop body can be consumed, and definitions in the body may not occur.
  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(body)))
      return false;

    type = control_type_add_branch(opt, type, body);
  }

  if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(else_clause)))
      return false;

    type = control_type_add_branch(opt, type, else_clause);
    ast_inheritbranch(ast, body);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  if(type == NULL)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_repeat(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* cond_type = ast_type(cond);

  if(is_typecheck_error(cond_type))
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(opt->check.errors, cond, "condition must be a Bool");
    return false;
  }

  // All consumes have to be in scope when the loop body finishes.
  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(body, body, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

  // No symbol status is inherited from the loop body or condition. Nothing
  // from outside can be consumed, and definitions inside may not occur.
  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(body)))
      return false;

    type = control_type_add_branch(opt, type, body);
  }

  if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(else_clause)))
      return false;

    type = control_type_add_branch(opt, type, else_clause);
    ast_inheritbranch(ast, else_clause);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  if(type == NULL)
    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

bool expr_try(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  ast_t* type = NULL;

  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(body)))
      return false;

    type = control_type_add_branch(opt, type, body);
  }

  if(!ast_checkflag(else_clause, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(else_clause)))
      return false;

    type = control_type_add_branch(opt, type, else_clause);
  }

  if(type == NULL)
  {
    if(ast_sibling(ast) != NULL)
    {
      ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
      return false;
    }

    ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  }

  // The then clause does not affect the type of the expression.
  if(ast_checkflag(then_clause, AST_FLAG_JUMPS_AWAY))
  {
    ast_error(opt->check.errors, then_clause,
      "then clause always terminates the function");
    return false;
  }

  ast_t* then_type = ast_type(then_clause);

  if(is_typecheck_error(then_type))
    return false;

  if(is_type_literal(then_type))
  {
    ast_error(opt->check.errors, then_clause,
      "Cannot infer type of unused literal");
    return false;
  }

  ast_settype(ast, type);

  literal_unify_control(ast, opt);

  // Push the symbol status from the then clause to our parent scope.
  ast_inheritstatus(ast_parent(ast), then_clause);
  return true;
}

bool expr_recover(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, expr);
  ast_t* type = ast_type(expr);

  if(is_typecheck_error(type))
    return false;

  if(is_type_literal(type))
  {
    make_literal_type(ast);
    return true;
  }

  ast_t* r_type = recover_type(type, ast_id(cap));

  if(r_type == NULL)
  {
    ast_error(opt->check.errors, ast, "can't recover to this capability");
    ast_error_continue(opt->check.errors, expr, "expression type is %s",
      ast_print_type(type));
    return false;
  }

  ast_settype(ast, r_type);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), expr);
  return true;
}

bool expr_break(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  if(t->frame->loop_body == NULL)
  {
    ast_error(opt->check.errors, ast, "must be in a loop");
    return false;
  }

  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(t->frame->loop_body, ast, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  // break is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);

  // Add type to loop.
  ast_t* body = ast_child(ast);

  if(ast_id(body) != TK_NONE)
  {
    if(ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
    {
      ast_error(opt->check.errors, body,
        "break value cannot be an expression that jumps away with no value");
      return false;
    }

    ast_t* loop_type = ast_type(t->frame->loop);

    // If there is no body the break will jump to the else branch, whose type
    // has already been added to the loop type.
    loop_type = control_type_add_branch(opt, loop_type, body);
    ast_settype(t->frame->loop, loop_type);
  }

  return true;
}

bool expr_continue(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  if(t->frame->loop_body == NULL)
  {
    ast_error(opt->check.errors, ast, "must be in a loop");
    return false;
  }

  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(t->frame->loop_body, ast, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  // continue is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  return true;
}

bool expr_return(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  // return is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_t* parent = ast_parent(ast);
  ast_t* current = ast;

  while(ast_id(parent) == TK_SEQ)
  {
    pony_assert(ast_childlast(parent) == current);
    current = parent;
    parent = ast_parent(parent);
  }

  if(current == t->frame->method_body)
  {
    ast_error(opt->check.errors, ast,
      "use return only to exit early from a method, not at the end");
    return false;
  }

  ast_t* type = ast_childidx(t->frame->method, 4);
  ast_t* body = ast_child(ast);

  if(!coerce_literals(&body, type, opt))
    return false;

  ast_t* body_type = ast_type(body);

  if(ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(body_type))
      return false;

    ast_error(opt->check.errors, body,
      "return value cannot be a control statement");
    return false;
  }

  bool ok = true;

  switch(ast_id(t->frame->method))
  {
    case TK_NEW:
      if(is_this_incomplete(t, ast))
      {
        ast_error(opt->check.errors, ast,
          "all fields must be defined before constructor returns");
        ok = false;
      }
      break;

    case TK_BE:
      pony_assert(is_none(body_type));
      break;

    default:
    {
      // The body type must be a subtype of the return type, and an alias of
      // the body type must be a subtype of an alias of the return type.
      ast_t* a_type = alias(type);
      ast_t* a_body_type = alias(body_type);

      errorframe_t info = NULL;
      if(!is_subtype(body_type, type, &info, opt) ||
        !is_subtype(a_body_type, a_type, &info, opt))
      {
        errorframe_t frame = NULL;
        ast_t* last = ast_childlast(body);
        ast_error_frame(&frame, last, "returned value isn't the return type");
        ast_error_frame(&frame, type, "function return type: %s",
          ast_print_type(type));
        ast_error_frame(&frame, body_type, "returned value type: %s",
          ast_print_type(body_type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        ok = false;
      }

      ast_free_unattached(a_type);
      ast_free_unattached(a_body_type);
    }
  }

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  return ok;
}

bool expr_error(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  // error is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  return true;
}

bool expr_compile_error(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  // compile_error is always the last expression in a sequence
  pony_assert(ast_sibling(ast) == NULL);

  ast_setflag(ast, AST_FLAG_JUMPS_AWAY);
  return true;
}

bool expr_location(pass_opt_t* opt, ast_t* ast)
{
  ast_settype(ast, type_builtin(opt, ast, "SourceLoc"));
  return true;
}
