#include "control.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
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

  ast_t* type = NULL;
  size_t branch_count = 0;

  if(l_type != NULL)
  {
    type = type_union(type, l_type);
    ast_inheritbranch(ast, left);
    branch_count++;
  }

  if(r_type != NULL)
  {
    type = type_union(type, r_type);
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
  ast_t* type = ast_type(ast);

  type = type_union(type, l_type);
  ast_inheritbranch(ast, left);
  size_t branch_count = 1;

  // TODO: A break statement in the body means some definitions might not
  // happen.
  if(r_type != NULL)
  {
    type = type_union(type, r_type);
    ast_inheritbranch(ast, left);
    branch_count++;
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  ast_consolidate_branches(ast, branch_count);

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
  ast_t* type = ast_type(ast);

  type = type_union(type, body_type);

  // TODO: doesn't work correctly: body has no scope, so consume/define is
  // going into the repeat node.
  // ast_inheritbranch(ast, body);
  size_t branch_count = 1;

  // TODO: A break statement in the body means some definitions might not
  // happen.
  if(else_type != NULL)
  {
    type = type_union(type, else_type);
    ast_inheritbranch(ast, else_clause);
    branch_count++;
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  ast_consolidate_branches(ast, branch_count);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
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
  ast_t* type = ast_type(body);
  ast_t* loop_type = ast_type(t->frame->loop);

  type = type_union(type, loop_type);
  ast_settype(t->frame->loop, type);

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

static bool is_method_result(ast_t* body, ast_t* ast)
{
  if(ast == body)
    return true;

  ast_t* parent = ast_parent(ast);

  if((ast_id(parent) == TK_SEQ) && (ast_sibling(ast) != NULL))
    return false;

  return is_method_result(body, parent);
}

bool expr_return(typecheck_t* t, ast_t* ast)
{
  if(t->frame->method_body == NULL)
  {
    ast_error(ast, "return must occur in a method body");
    return false;
  }

  switch(ast_id(t->frame->method))
  {
    case TK_NEW:
      ast_error(ast, "can't return a value in a constructor");
      return false;

    case TK_BE:
      ast_error(ast, "can't return a value in a behaviour");
      return false;

    default: {}
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "is followed with this expression");
    return false;
  }

  if(is_method_result(t->frame->method_body, ast))
  {
    ast_error(ast,
      "use return only to exit early from a method, not at the end");
    return false;
  }

  ast_t* type = ast_childidx(t->frame->method, 4);
  ast_t* body = ast_child(ast);

  if(!coerce_literals(body, type))
    return false;

  // The body type must match the return type, without subsumption, or an alias
  // of the body type must be a subtype of the return type.
  ast_t* body_type = ast_type(body);
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
