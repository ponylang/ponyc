#include "expr.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../expr/reference.h"
#include "../expr/operator.h"
#include "../expr/postfix.h"
#include "../expr/control.h"
#include "../expr/match.h"
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

ast_result_t pass_expr(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!expr_nominal(astp))
        return AST_FATAL;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!expr_field(ast))
        return AST_FATAL;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!expr_fun(ast))
        return AST_FATAL;
      break;

    case TK_SEQ:
      if(!expr_seq(ast))
        return AST_FATAL;
      break;

    case TK_VAR:
    case TK_LET:
      if(!expr_local(ast))
        return AST_FATAL;
      break;

    case TK_IDSEQ:
      if(!expr_idseq(ast))
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

    case TK_IS:
    case TK_ISNT:
      if(!expr_identity(ast))
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

    case TK_TUPLE:
      if(!expr_tuple(ast))
        return AST_FATAL;
      break;

    case TK_ARRAY:
      // TODO: determine our type by looking at every expr in the array
      ast_error(ast, "not implemented (array)");
      return AST_FATAL;

    case TK_OBJECT:
      // TODO: create a type for the object, make sure it fulfills any traits it
      // claims to have
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

    case TK_TRUE:
    case TK_FALSE:
      if(!expr_literal(ast, "Bool"))
        return AST_FATAL;
      break;

    case TK_INT:
      // Integer literals can be integers or floats
      ast_settype(ast, ast_from(ast, TK_NUMBERLITERAL));
      break;

    case TK_FLOAT:
      ast_settype(ast, ast_from(ast, TK_FLOATLITERAL));
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

    case TK_FFICALL:
      if(!expr_ffi(ast))
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
