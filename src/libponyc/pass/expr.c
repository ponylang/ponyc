#include "expr.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../expr/reference.h"
#include "../expr/operator.h"
#include "../expr/postfix.h"
#include "../expr/call.h"
#include "../expr/control.h"
#include "../expr/match.h"
#include "../expr/array.h"
#include "../expr/ffi.h"
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
    case TK_TRY2:
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
    case TK_TRY2:
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

ast_result_t pass_expr(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!expr_nominal(options, astp))
        return AST_FATAL;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!expr_field(options, ast))
        return AST_FATAL;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!expr_fun(options, ast))
        return AST_FATAL;
      break;

    case TK_SEQ:
      if(!expr_seq(ast))
        return AST_FATAL;
      break;

    case TK_VAR:
    case TK_LET:
      if(!expr_local(t, ast))
        return AST_FATAL;
      break;

    case TK_IDSEQ:
      if(!expr_idseq(ast))
        return AST_FATAL;
      break;

    case TK_BREAK:
      if(!expr_break(t, ast))
        return AST_FATAL;
      break;

    case TK_CONTINUE:
      if(!expr_continue(t, ast))
        return AST_FATAL;
      break;

    case TK_RETURN:
      if(!expr_return(options, ast))
        return AST_FATAL;
      break;

    case TK_IS:
    case TK_ISNT:
      if(!expr_identity(ast))
        return AST_FATAL;
      break;

    case TK_ASSIGN:
      if(!expr_assign(options, ast))
        return AST_FATAL;
      break;

    case TK_CONSUME:
      if(!expr_consume(t, ast))
        return AST_FATAL;
      break;

    case TK_RECOVER:
      if(!expr_recover(ast))
        return AST_FATAL;
      break;

    case TK_DOT:
      if(!expr_dot(options, ast))
        return AST_FATAL;
      break;

    case TK_TILDE:
      if(!expr_tilde(options, ast))
        return AST_FATAL;
      break;

    case TK_QUALIFY:
      if(!expr_qualify(options, ast))
        return AST_FATAL;
      break;

    case TK_CALL:
      if(!expr_call(options, astp))
        return AST_FATAL;
      break;

    case TK_IF:
      if(!expr_if(ast))
        return AST_FATAL;
      break;

    case TK_WHILE:
      if(!expr_while(ast))
        return AST_FATAL;
      break;

    case TK_REPEAT:
      if(!expr_repeat(ast))
        return AST_FATAL;
      break;

    case TK_TRY:
    case TK_TRY2:
      if(!expr_try(ast))
        return AST_FATAL;
      break;

    case TK_MATCH:
      if(!expr_match(ast))
        return AST_FATAL;
      break;

    case TK_CASES:
      if(!expr_cases(ast))
        return AST_FATAL;
      break;

    case TK_CASE:
      if(!expr_case(options, ast))
        return AST_FATAL;
      break;

    case TK_TUPLE:
      if(!expr_tuple(ast))
        return AST_FATAL;
      break;

    case TK_ARRAY:
      if(!expr_array(options, astp))
        return AST_FATAL;
      break;

    case TK_REFERENCE:
      if(!expr_reference(options, ast))
        return AST_FATAL;
      break;

    case TK_THIS:
      if(!expr_this(options, ast))
        return AST_FATAL;
      break;

    case TK_TRUE:
    case TK_FALSE:
      if(!expr_literal(ast, "Bool"))
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

      if(!expr_literal(ast, "String"))
        return AST_FATAL;
      break;

    case TK_ERROR:
      if(!expr_error(ast))
        return AST_FATAL;
      break;

    case TK_COMPILER_INTRINSIC:
      if(!expr_compiler_intrinsic(t, ast))
        return AST_FATAL;
      break;

    case TK_POSITIONALARGS:
    case TK_NAMEDARGS:
    case TK_NAMEDARG:
      ast_inheriterror(ast);
      break;

    case TK_FFICALL:
      if(!expr_ffi(options, ast))
        return AST_FATAL;
      break;

    case TK_AMP:
      if(!expr_addressof(ast))
        return AST_FATAL;
      break;

    case TK_DONTCARE:
      if(!expr_dontcare(ast))
        return AST_FATAL;
      break;

    default: {}
  }

  return AST_OK;
}
