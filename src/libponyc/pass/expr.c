#include "expr.h"
#include "../expr/literal.h"
#include "../expr/reference.h"
#include "../expr/operator.h"
#include "../expr/postfix.h"
#include "../expr/call.h"
#include "../expr/control.h"
#include "../expr/match.h"
#include "../expr/array.h"
#include "../expr/ffi.h"
#include "../expr/lambda.h"
#include <assert.h>

bool is_result_needed(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_SEQ:
      // If we're not the last element, we don't need the result.
      if(ast_sibling(ast) != NULL)
        return false;

      return is_result_needed(parent);

    case TK_IF:
    case TK_WHILE:
    case TK_MATCH:
      // Condition needed, body/else needed only if parent needed.
      if(ast_child(parent) == ast)
        return true;

      return is_result_needed(parent);

    case TK_REPEAT:
      // Cond needed, body/else needed only if parent needed.
      if(ast_childidx(parent, 1) == ast)
        return true;

      return is_result_needed(parent);

    case TK_CASE:
      // Pattern, guard needed, body needed only if parent needed
      if(ast_childidx(parent, 2) != ast)
        return true;

      return is_result_needed(parent);

    case TK_CASES:
    case TK_TRY:
    case TK_TRY_NO_CHECK:
      // Only if parent needed.
      return is_result_needed(parent);

    case TK_NEW:
    case TK_BE:
      // Result of a constructor or behaviour isn't needed.
      return false;

    default: {}
  }

  // All others needed.
  return true;
}

bool is_method_result(typecheck_t* t, ast_t* ast)
{
  if(ast == t->frame->method_body)
    return true;

  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_SEQ:
      // More expressions in a sequence means we're not the result.
      if(ast_sibling(ast) != NULL)
        return false;
      break;

    case TK_IF:
    case TK_WHILE:
    case TK_MATCH:
      // The condition is not the result.
      if(ast_child(parent) == ast)
        return false;
      break;

    case TK_REPEAT:
      // The condition is not the result.
      if(ast_childidx(parent, 1) == ast)
        return false;
      break;

    case TK_CASE:
      // The pattern and the guard are not the result.
      if(ast_childidx(parent, 2) != ast)
        return false;
      break;

    case TK_CASES:
    case TK_RECOVER:
      // These can be results.
      break;

    case TK_TRY:
    case TK_TRY_NO_CHECK:
      // The finally block is not the result.
      if(ast_childidx(parent, 2) == ast)
        return false;
      break;

    default:
      // Other expressions are not results.
      return false;
  }

  return is_method_result(t, parent);
}

bool is_method_return(typecheck_t* t, ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  if(ast_id(parent) == TK_SEQ)
  {
    parent = ast_parent(parent);

    if(ast_id(parent) == TK_RETURN)
      return true;
  }

  return is_method_result(t, ast);
}

bool is_typecheck_error(ast_t* type)
{
  if(type == NULL)
    return true;

  if(ast_id(type) == TK_INFERTYPE || ast_id(type) == TK_ERRORTYPE)
    return true;

  return false;
}

bool is_control_type(ast_t* type)
{
  if(type == NULL)
    return true;

  switch(ast_id(type))
  {
    case TK_IF:
    case TK_TRY:
    case TK_MATCH:
    case TK_CASES:
    case TK_BREAK:
    case TK_CONTINUE:
    case TK_RETURN:
    case TK_ERROR:
      return true;

    default: {}
  }

  return false;
}

ast_result_t pass_pre_expr(ast_t** astp, pass_opt_t* options)
{
  (void)options;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_PRESERVE:
      return AST_IGNORE;

    case TK_USE:
      // Don't look in use commands to avoid false type errors from the guard
      return AST_IGNORE;

    default: {}
  }

  return AST_OK;
}

ast_result_t pass_expr(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:    r = expr_nominal(options, astp); break;
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_PARAM:      r = expr_field(options, ast); break;
    case TK_NEW:
    case TK_BE:
    case TK_FUN:        r = expr_fun(options, ast); break;
    case TK_SEQ:        r = expr_seq(options, ast); break;
    case TK_VAR:
    case TK_LET:        r = expr_local(t, ast); break;
    case TK_BREAK:      r = expr_break(t, ast); break;
    case TK_CONTINUE:   r = expr_continue(t, ast); break;
    case TK_RETURN:     r = expr_return(options, ast); break;
    case TK_IS:
    case TK_ISNT:       r = expr_identity(options, ast); break;
    case TK_ASSIGN:     r = expr_assign(options, ast); break;
    case TK_CONSUME:    r = expr_consume(t, ast); break;
    case TK_RECOVER:    r = expr_recover(ast); break;
    case TK_DOT:        r = expr_dot(options, astp); break;
    case TK_TILDE:      r = expr_tilde(options, astp); break;
    case TK_QUALIFY:    r = expr_qualify(options, astp); break;
    case TK_CALL:       r = expr_call(options, astp); break;
    case TK_IF:         r = expr_if(options, ast); break;
    case TK_WHILE:      r = expr_while(options, ast); break;
    case TK_REPEAT:     r = expr_repeat(options, ast); break;
    case TK_TRY_NO_CHECK:
    case TK_TRY:        r = expr_try(options, ast); break;
    case TK_MATCH:      r = expr_match(options, ast); break;
    case TK_CASES:      r = expr_cases(ast); break;
    case TK_CASE:       r = expr_case(options, ast); break;
    case TK_TUPLE:      r = expr_tuple(ast); break;
    case TK_ARRAY:      r = expr_array(options, astp); break;
    case TK_REFERENCE:  r = expr_reference(options, astp); break;
    case TK_THIS:       r = expr_this(options, ast); break;
    case TK_TRUE:
    case TK_FALSE:      r = expr_literal(options, ast, "Bool"); break;
    case TK_ERROR:      r = expr_error(ast); break;
    case TK_COMPILER_INTRINSIC:
                        r = expr_compiler_intrinsic(t, ast); break;
    case TK_POSITIONALARGS:
    case TK_NAMEDARGS:
    case TK_NAMEDARG:
    case TK_UPDATEARG:  ast_inheritflags(ast); break;
    case TK_AMP:        r = expr_addressof(options, ast); break;
    case TK_IDENTITY:   r = expr_identityof(options, ast); break;
    case TK_DONTCARE:   r = expr_dontcare(ast); break;

    case TK_LAMBDA:
      if(!expr_lambda(options, astp))
        return AST_FATAL;
      break;

    case TK_INT:
      // Integer literals can be integers or floats
      make_literal_type(ast);
      break;

    case TK_FLOAT:
      make_literal_type(ast);
      break;

    case TK_STRING:
      if(ast_id(ast_parent(ast)) == TK_PACKAGE)
        return AST_OK;

      r = expr_literal(options, ast, "String");
      break;

    case TK_FFICALL:
      return expr_ffi(options, ast);

    default: {}
  }

  if(!r)
  {
    assert(get_error_count() > 0);
    return AST_ERROR;
  }

  // Can't use ast here, it might have changed
  symtab_t* symtab = ast_get_symtab(*astp);

  if(symtab != NULL && !symtab_check_all_defined(symtab))
    return AST_ERROR;

  return AST_OK;
}

ast_result_t pass_nodebug(ast_t** astp, pass_opt_t* options)
{
  (void)options;
  ast_setdebug(*astp, false);
  return AST_OK;
}
