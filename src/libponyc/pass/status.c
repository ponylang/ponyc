#include "status.h"
#include "expr.h"
#include "../expr/literal.h"
#include "../type/lookup.h"
#include <assert.h>

static bool check_assigned_id(pass_opt_t* opt, ast_t* ast, bool let,
  bool need_value)
{
  assert(ast_id(ast) == TK_ID);
  const char* name = ast_name(ast);

  sym_status_t status;
  ast_get(ast, name, &status);

  switch(status)
  {
    case SYM_UNDEFINED:
      if(need_value)
      {
        ast_error(opt->check.errors, ast,
          "the left side is undefined but its value is used");
        return false;
      }

      ast_setstatus(ast, name, SYM_DEFINED);
      return true;

    case SYM_DEFINED:
      if(let)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let or embed definition more than once");
        return false;
      }

      return true;

    case SYM_CONSUMED:
    {
      bool ok = true;

      if(need_value)
      {
        ast_error(opt->check.errors, ast,
          "the left side is consumed but its value is used");
        ok = false;
      }

      if(let)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let or embed definition more than once");
        ok = false;
      }

      if(opt->check.frame->try_expr != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't reassign to a consumed identifier in a try expression");
        ok = false;
      }

      if(ok)
        ast_setstatus(ast, name, SYM_DEFINED);

      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool check_assigned_lvalue(pass_opt_t* opt, ast_t* ast, bool need_value)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
      return true;

    case TK_VAR:
      return check_assigned_id(opt, ast_child(ast), false, need_value);

    case TK_LET:
      return check_assigned_id(opt, ast_child(ast), true, need_value);

    case TK_VARREF:
      return check_assigned_id(opt, ast_child(ast), false, need_value);

    case TK_LETREF:
    {
      ast_error(opt->check.errors, ast, "can't assign to a let local");
      return false;
    }

    case TK_FVARREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
        return true;

      return check_assigned_id(opt, right, false, need_value);
    }

    case TK_FLETREF:
    case TK_EMBEDREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
        return false;

      return check_assigned_id(opt, right, true, need_value);
    }

    case TK_TUPLE:
    {
      // For tuple lvalues, we check each component expression as an lvalue.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        // Each child of the tuple is a single-child sequence, where the single
        // child of the sequence is the expression to be checked as an lvalue.
        assert(ast_id(child) == TK_SEQ);
        ast_t* inner = ast_child(child);
        assert(ast_sibling(inner) == NULL);

        if(!check_assigned_lvalue(opt, inner, need_value))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    default: {}
  }

  return false;
}

static bool is_this_incomplete(pass_opt_t* opt, ast_t* ast)
{
  // If we're in a default field initialiser, we're incomplete by definition.
  if(opt->check.frame->method == NULL)
    return true;

  // If we're not in a constructor, we're complete by definition.
  if(ast_id(opt->check.frame->method) != TK_NEW)
    return false;

  // Check if all fields have been marked as defined.
  ast_t* members = ast_childidx(opt->check.frame->type, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
      case TK_EMBED:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_get(ast, ast_name(id), &status);

        if(status != SYM_DEFINED)
          return true;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return false;
}

static bool is_assigned_to(ast_t* ast, bool check_result_needed)
{
  while(true)
  {
    ast_t* parent = ast_parent(ast);

    switch(ast_id(parent))
    {
      case TK_ASSIGN:
      {
        // Has to be the left hand side of an assignment. Left and right sides
        // are swapped, so we must be the second child.
        if(ast_childidx(parent, 1) != ast)
          return false;

        if(!check_result_needed)
          return true;

        // The result of that assignment can't be used.
        return !is_result_needed(parent);
      }

      case TK_SEQ:
      {
        // Might be in a tuple on the left hand side.
        if(ast_childcount(parent) > 1)
          return false;

        break;
      }

      case TK_TUPLE:
        break;

      default:
        return false;
    }

    ast = parent;
  }
}

static bool is_constructed_from(pass_opt_t* opt, ast_t* ast, ast_t* type)
{
  ast_t* parent = ast_parent(ast);

  if(ast_id(parent) != TK_DOT)
    return false;

  AST_GET_CHILDREN(parent, left, right);
  ast_t* find = lookup_try(opt, parent, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ok = ast_id(find) == TK_NEW;
  ast_free_unattached(find);
  return ok;
}

static bool valid_reference(pass_opt_t* opt, ast_t* ast, ast_t* type,
  sym_status_t status)
{
  if(is_constructed_from(opt, ast, type))
    return true;

  switch(status)
  {
    case SYM_DEFINED:
      return true;

    case SYM_CONSUMED:
      if(is_assigned_to(ast, true))
        return true;

      ast_error(opt->check.errors, ast,
        "can't use a consumed local in an expression");
      return false;

    case SYM_UNDEFINED:
      if(is_assigned_to(ast, true))
        return true;

      ast_error(opt->check.errors, ast,
        "can't use an undefined variable in an expression");
      return false;

    default: {}
  }

  assert(0);
  return false;
}

static bool status_localref(pass_opt_t* opt, ast_t* ast)
{
  assert(
    (ast_id(ast) == TK_LETREF) || (ast_id(ast) == TK_VARREF) ||
    (ast_id(ast) == TK_PARAMREF));

  const char* name = ast_name(ast_child(ast));

  sym_status_t status;
  ast_get(ast, name, &status);

  if(!valid_reference(opt, ast, ast_type(ast), status))
    return false;

  return true;
}

static bool status_fieldref(pass_opt_t* opt, ast_t* ast)
{
  assert(
    (ast_id(ast) == TK_FLETREF) || (ast_id(ast) == TK_FVARREF) ||
    (ast_id(ast) == TK_EMBEDREF));
  AST_GET_CHILDREN(ast, left, right);

  // Handle symbol status if the left side is 'this'.
  if(ast_id(left) == TK_THIS)
  {
    const char* name = ast_name(right);

    sym_status_t status;
    ast_get(ast, name, &status);

    if(!valid_reference(opt, ast, ast_type(ast), status))
      return false;
  }

  return true;
}

static bool status_assign(pass_opt_t* opt, ast_t* ast)
{
  // Left and right are swapped in the AST to make sure we type check the
  // right side before the left. Fetch them in the opposite order.
  assert(ast_id(ast) == TK_ASSIGN);
  AST_GET_CHILDREN(ast, right, left);

  if(!check_assigned_lvalue(opt, left, is_result_needed(ast)))
  {
    ast_error(opt->check.errors, ast,
      "left side must be something that can be assigned to");
    return false;
  }

  return true;
}

static bool status_consume(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, term);

  const char* name = NULL;

  switch(ast_id(term))
  {
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
    {
      ast_t* id = ast_child(term);
      name = ast_name(id);
      break;
    }

    case TK_THIS:
    {
      name = stringtab("this");
      break;
    }

    default:
      ast_error(opt->check.errors, ast,
        "consume must take 'this', a local, or a parameter");
      return false;
  }

  // Can't consume from an outer scope while in a loop condition.
  if((opt->check.frame->loop_cond != NULL) &&
    !ast_within_scope(opt->check.frame->loop_cond, ast, name))
  {
    ast_error(opt->check.errors, ast,
      "can't consume from an outer scope in a loop condition");
    return false;
  }

  ast_setstatus(ast, name, SYM_CONSUMED);

  return true;
}

static bool status_seq(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert(ast_id(ast) == TK_SEQ);

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

static bool status_if(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert((ast_id(ast) == TK_IF) || (ast_id(ast) == TK_IFDEF));
  AST_GET_CHILDREN(ast, cond, left, right);

  size_t branch_count = 0;

  if(!is_control_type(ast_type(left)))
  {
    ast_inheritbranch(ast, left);
    branch_count++;
  }

  if(!is_control_type(ast_type(right)))
  {
    ast_inheritbranch(ast, right);
    branch_count++;
  }

  ast_consolidate_branches(ast, branch_count);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool status_while(pass_opt_t* opt, ast_t* ast)
{
  assert(ast_id(ast) == TK_WHILE);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  // All consumes have to be in scope when the loop body finishes.
  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(body, body, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  if(!is_control_type(ast_type(else_clause)))
  {
    ast_inheritbranch(ast, body);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool status_repeat(pass_opt_t* opt, ast_t* ast)
{
  assert(ast_id(ast) == TK_REPEAT);
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  // All consumes have to be in scope when the loop body finishes.
  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(body, body, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  if(!is_control_type(ast_type(else_clause)))
  {
    ast_inheritbranch(ast, else_clause);

    // Use a branch count of two instead of one. This means we will pick up any
    // consumes, but not any definitions, since definitions may not occur.
    ast_consolidate_branches(ast, 2);
  }

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);

  return true;
}

static bool status_try(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert(ast_id(ast) == TK_TRY);
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  // Push the symbol status from the then clause to our parent scope.
  ast_inheritstatus(ast_parent(ast), then_clause);

  return true;
}

static bool status_recover(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert(ast_id(ast) == TK_RECOVER);
  AST_GET_CHILDREN(ast, cap, expr);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), expr);
  return true;
}

static bool status_break_or_continue(pass_opt_t* opt, ast_t* ast)
{
  assert((ast_id(ast) == TK_BREAK) || (ast_id(ast) == TK_CONTINUE));

  errorframe_t errorf = NULL;
  if(!ast_all_consumes_in_scope(opt->check.frame->loop_body, ast, &errorf))
  {
    errorframe_report(&errorf, opt->check.errors);
    return false;
  }

  return true;
}

static bool status_return(pass_opt_t* opt, ast_t* ast)
{
  assert(ast_id(ast) == TK_RETURN);

  if(ast_id(opt->check.frame->method) == TK_NEW && is_this_incomplete(opt, ast))
    ast_error(opt->check.errors, ast,
      "all fields must be defined before constructor returns");
    return false;

  return true;
}

static bool status_match(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert(ast_id(ast) == TK_MATCH);
  AST_GET_CHILDREN(ast, expr, cases, else_clause);

  size_t branch_count = 0;

  if(!is_control_type(ast_type(cases)))
  {
    ast_inheritbranch(ast, cases);
    branch_count++;
  }

  if(!is_control_type(ast_type(else_clause)))
  {
    ast_inheritbranch(ast, else_clause);
    branch_count++;
  }

  ast_consolidate_branches(ast, branch_count);

  // Push our symbol status to our parent scope.
  ast_inheritstatus(ast_parent(ast), ast);
  return true;
}

static bool status_cases(pass_opt_t* opt, ast_t* ast)
{
  (void)opt;
  assert(ast_id(ast) == TK_CASES);
  ast_t* the_case = ast_child(ast);
  assert(the_case);

  size_t branch_count = 0;

  while(the_case != NULL)
  {
    AST_GET_CHILDREN(the_case, pattern, guard, body);
    ast_t* body_type = ast_type(body);

    if(!is_typecheck_error(body_type) && !is_control_type(body_type))
    {
      ast_inheritbranch(ast, the_case);
      branch_count++;
    }

    the_case = ast_sibling(the_case);
  }

  ast_consolidate_branches(ast, branch_count);

  return true;
}

bool status_new(pass_opt_t* opt, ast_t* ast)
{
  assert(ast_id(ast) == TK_NEW);

  ast_t* members = ast_parent(ast);
  ast_t* member = ast_child(members);
  bool result = true;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_t* def = ast_get(ast, ast_name(id), &status);

        if((def != member) || (status != SYM_DEFINED))
        {
          ast_error(opt->check.errors, def,
            "field left undefined in constructor");
          result = false;
        }

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(!result)
    ast_error(opt->check.errors, ast,
      "constructor with undefined fields is here");

  return result;
}

ast_result_t pass_status(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_LETREF:
    case TK_VARREF:
    case TK_PARAMREF:       r = status_localref(options, ast); break;
    case TK_FLETREF:
    case TK_FVARREF:
    case TK_EMBEDREF:       r = status_fieldref(options, ast); break;
    case TK_ASSIGN:         r = status_assign(options, ast); break;
    case TK_CONSUME:        r = status_consume(options, ast); break;
    case TK_SEQ:            r = status_seq(options, ast); break;
    case TK_IF:
    case TK_IFDEF:          r = status_if(options, ast); break;
    case TK_WHILE:          r = status_while(options, ast); break;
    case TK_REPEAT:         r = status_repeat(options, ast); break;
    case TK_TRY:            r = status_try(options, ast); break;
    case TK_RECOVER:        r = status_recover(options, ast); break;
    case TK_BREAK:
    case TK_CONTINUE:       r = status_break_or_continue(options, ast); break;
    case TK_RETURN:         r = status_return(options, ast); break;
    case TK_MATCH:          r = status_match(options, ast); break;
    case TK_CASES:          r = status_cases(options, ast); break;
    case TK_NEW:            r = status_new(options, ast); break;

    default: {}
  }

  if(!r)
  {
    assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  // Can't use ast here, it might have changed
  symtab_t* symtab = ast_get_symtab(*astp);

  if(symtab != NULL && !symtab_check_all_defined(symtab, options->check.errors))
    return AST_ERROR;

  return AST_OK;
}
