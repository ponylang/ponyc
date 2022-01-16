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
#include "../type/typeparam.h"
#include "ponyassert.h"

bool expr_seq(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_SEQ);
  ast_t* parent = ast_parent(ast);

  // This sequence will be handled when the array literal is handled.
  if(ast_id(parent) == TK_ARRAY)
    return true;

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

  if(!ok)
    return false;

  // We might already have a type due to a return expression.
  ast_t* type = ast_type(ast);
  ast_t* last = ast_childlast(ast);

  if((type != NULL) && !coerce_literals(&last, type, opt))
    return false;

  // Type is unioned with the type of the last child.
  type = control_type_add_branch(opt, type, last);
  ast_settype(ast, type);

  return true;
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
  pony_assert((ast_id(ast) == TK_IF) || (ast_id(ast) == TK_IFDEF));
  AST_GET_CHILDREN(ast, cond, left, right);

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

  if(!ast_checkflag(left, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(left)))
      return false;

    type = control_type_add_branch(opt, type, left);
  }

  if(!ast_checkflag(right, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(right)))
      return false;

    type = control_type_add_branch(opt, type, right);
  }

  if(type == NULL)
  {
    if((ast_id(ast_parent(ast)) == TK_SEQ) && ast_sibling(ast) != NULL)
    {
      ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
      return false;
    }
  }

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  if(ast_id(ast) == TK_IFDEF)
    return resolve_ifdef(opt, ast);

  return true;
}

bool expr_iftype(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_IFTYPE_SET);
  AST_GET_CHILDREN(ast, left_control, right);
  AST_GET_CHILDREN(left_control, sub, super, left);

  ast_t* type = NULL;

  if(!ast_checkflag(left, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(left)))
      return false;

    type = control_type_add_branch(opt, type, left);
  }

  if(!ast_checkflag(right, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(right)))
      return false;

    type = control_type_add_branch(opt, type, right);
  }

  if(type == NULL)
  {
    if((ast_id(ast_parent(ast)) == TK_SEQ) && ast_sibling(ast) != NULL)
    {
      ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
      return false;
    }
  }

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  return true;
}

bool expr_while(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_WHILE);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  ast_t* cond_type = ast_type(cond);

  if(is_typecheck_error(cond_type))
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(opt->check.errors, cond, "condition must be a Bool");
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

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

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  return true;
}

bool expr_repeat(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_REPEAT);
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* cond_type = ast_type(cond);

  if(is_typecheck_error(cond_type))
    return false;

  if(!is_bool(cond_type))
  {
    ast_error(opt->check.errors, cond, "condition must be a Bool");
    return false;
  }

  // Union with any existing type due to a break expression.
  ast_t* type = ast_type(ast);

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

  ast_settype(ast, type);
  literal_unify_control(ast, opt);

  return true;
}

bool expr_try(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_TRY) || (ast_id(ast) == TK_TRY_NO_CHECK));
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

  if((type == NULL) && (ast_sibling(ast) != NULL))
  {
    ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
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

  return true;
}

bool expr_disposing_block(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_DISPOSING_BLOCK);
  AST_GET_CHILDREN(ast, body, dispose_clause);

  ast_t* type = NULL;

  if(!ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
  {
    if(is_typecheck_error(ast_type(body)))
      return false;

    type = control_type_add_branch(opt, type, body);
  }

  if((type == NULL) && (ast_sibling(ast) != NULL))
  {
    ast_error(opt->check.errors, ast_sibling(ast), "unreachable code");
    return false;
  }

  ast_t* dispose_type = ast_type(dispose_clause);

  if(is_typecheck_error(dispose_type))
    return false;

  if(is_type_literal(dispose_type))
  {
    ast_error(opt->check.errors, dispose_clause,
      "Cannot infer type of unused literal");
    return false;
  }

  ast_settype(ast, type);

  literal_unify_control(ast, opt);

  return true;
}

bool expr_recover(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_RECOVER);
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

  return true;
}

bool expr_break(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_BREAK);

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

    ast_t* loop_type = ast_type(opt->check.frame->loop);

    // If there is no body the break will jump to the else branch, whose type
    // has already been added to the loop type.
    loop_type = control_type_add_branch(opt, loop_type, body);
    ast_settype(opt->check.frame->loop, loop_type);
  }

  return true;
}

bool is_local_or_param(ast_t* ast)
{
  while(ast_id(ast) == TK_SEQ)
  {
    ast = ast_child(ast);
    if (ast_sibling(ast) != NULL)
      return false;
  }
  switch (ast_id(ast))
  {
    case TK_PARAMREF:
    case TK_LETREF:
    case TK_VARREF:
      return true;

    case TK_REFERENCE:
      pony_assert(0);

    default: {}
  }
  return false;
}

bool expr_return(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_RETURN);

  ast_t* parent = ast_parent(ast);
  ast_t* current = ast;

  while(ast_id(parent) == TK_SEQ)
  {
    pony_assert(ast_childlast(parent) == current);
    current = parent;
    parent = ast_parent(parent);
  }

  if(current == opt->check.frame->method_body)
  {
    ast_error(opt->check.errors, ast,
      "use return only to exit early from a method, not at the end");
    return false;
  }

  ast_t* type = ast_childidx(opt->check.frame->method, 4);
  ast_t* body = ast_child(ast);

  if(!coerce_literals(&body, type, opt))
    return false;

  ast_t* body_type = ast_type(body);

  if(is_typecheck_error(body_type))
    return false;

  if(ast_checkflag(body, AST_FLAG_JUMPS_AWAY))
  {
    ast_error(opt->check.errors, body,
      "return value cannot be a control statement");
    return false;
  }

  bool ok = true;

  switch(ast_id(opt->check.frame->method))
  {
    case TK_NEW:
      pony_assert(ast_id(ast_child(ast)) == TK_THIS);
      break;

    case TK_BE:
      pony_assert(is_none(body_type));
      break;

    default:
    {
      errorframe_t info = NULL;

      // If the experession is a local or parameter, auto-consume
      ast_t* r_type = NULL;
      if (is_local_or_param(body))
      {
          r_type = consume_type(body_type, TK_NONE);
          if (r_type != NULL)
          {
            // n.b. r_type should almost never be NULL
            // But it might be due to current unsoundness of generics.
            // Until we get a #stable cap constriant or otherwise,
            // we might reify a generic so that we have e.g. iso^ variables.
            body_type = r_type;
          }
      }
      if(!is_subtype(body_type, type, &info, opt))
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

      if (r_type != NULL) {
        ast_free_unattached(r_type);
      }
    }
  }

  return ok;
}

bool expr_location(pass_opt_t* opt, ast_t* ast)
{
  ast_settype(ast, type_builtin(opt, ast, "SourceLoc"));
  return true;
}
