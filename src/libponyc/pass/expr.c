#include "expr.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../expr/reference.h"
#include "../expr/operator.h"
#include "../expr/postfix.h"
#include "../expr/control.h"
#include "../expr/match.h"
#include <assert.h>

ast_result_t pass_expr(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!expr_field(ast))
        return AST_FATAL;
      break;

    case TK_NEW:
      // TODO: check that the object is fully initialised
      if(!expr_fun(ast))
        return AST_FATAL;
      break;

    case TK_BE:
    case TK_FUN:
      if(!expr_fun(ast))
        return AST_FATAL;
      break;

    case TK_SEQ:
      if(!expr_seq(ast))
        return AST_FATAL;
      break;

    case TK_IDSEQ:
      if(!expr_local(ast))
        return AST_FATAL;
      break;

    case TK_BREAK:
      if(!expr_break(ast))
        return AST_FATAL;
      break;

    case TK_CONTINUE:
      if(!expr_continue(ast))
        return AST_FATAL;
      break;

    case TK_RETURN:
      if(!expr_return(ast))
        return AST_FATAL;
      break;

    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
    case TK_PLUS:
      if(!expr_arithmetic(ast))
        return AST_FATAL;
      break;

    case TK_MINUS:
      if(!expr_minus(ast))
        return AST_FATAL;
      break;

    case TK_LSHIFT:
    case TK_RSHIFT:
      if(!expr_shift(ast))
        return AST_FATAL;
      break;

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      if(!expr_order(astp))
        return AST_FATAL;
      break;

    case TK_EQ:
    case TK_NE:
      if(!expr_compare(astp))
        return AST_FATAL;
      break;

    case TK_IS:
    case TK_ISNT:
      if(!expr_identity(ast))
        return AST_FATAL;
      break;

    case TK_AND:
    case TK_XOR:
    case TK_OR:
      if(!expr_logical(ast))
        return AST_FATAL;
      break;

    case TK_NOT:
      if(!expr_not(ast))
        return AST_FATAL;
      break;

    case TK_ASSIGN:
      if(!expr_assign(ast))
        return AST_FATAL;
      break;

    case TK_CONSUME:
      if(!expr_consume(ast))
        return AST_FATAL;
      break;

    case TK_RECOVER:
      if(!expr_recover(ast))
        return AST_FATAL;
      break;

    case TK_DOT:
      if(!expr_dot(ast))
        return AST_FATAL;
      break;

    case TK_QUALIFY:
      if(!expr_qualify(ast))
        return AST_FATAL;
      break;

    case TK_CALL:
      if(!expr_call(ast))
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
      if(!expr_case(ast))
        return AST_FATAL;
      break;

    case TK_AS:
      if(!expr_as(ast))
        return AST_FATAL;
      break;

    case TK_TUPLE:
      if(!expr_tuple(ast))
        return AST_FATAL;
      break;

    case TK_ARRAY:
      // TODO: determine our type by looking at every expr in the array
      ast_error(ast, "not implemented (array)");
      return AST_FATAL;

    case TK_OBJECT:
      // TODO: create a structural type for the object
      // TODO: make sure it fulfills any traits it claims to have
      ast_error(ast, "not implemented (object)");
      return AST_FATAL;

    case TK_REFERENCE:
      if(!expr_reference(ast))
        return AST_FATAL;
      break;

    case TK_THIS:
      if(!expr_this(ast))
        return AST_FATAL;
      break;

    case TK_INT:
      if(!expr_literal(ast, "IntLiteral"))
        return AST_FATAL;
      break;

    case TK_FLOAT:
      if(!expr_literal(ast, "FloatLiteral"))
        return AST_FATAL;
      break;

    case TK_STRING:
      if(!expr_literal(ast, "String"))
        return AST_FATAL;
      break;

    case TK_ERROR:
      if(!expr_error(ast))
        return AST_FATAL;
      break;

    case TK_COMPILER_INTRINSIC:
      if(!expr_compiler_intrinsic(ast))
        return AST_FATAL;
      break;

    case TK_POSITIONALARGS:
    case TK_NAMEDARGS:
    case TK_NAMEDARG:
      ast_inheriterror(ast);
      break;

    default: {}
  }

  return AST_OK;
}
