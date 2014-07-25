#include "genexpr.h"
#include "gencontrol.h"
#include "genoperator.h"
#include "genreference.h"
#include "../type/subtype.h"
#include <assert.h>

LLVMValueRef gen_expr(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_SEQ:
      return gen_seq(c, ast);

    case TK_FVARREF:
    case TK_FLETREF:
      return gen_fieldload(c, ast);

    case TK_PARAMREF:
      return gen_param(c, ast);

    case TK_VAR:
    case TK_LET:
    {
      ast_error(ast, "not implemented (codegen for local decl)");
      return NULL;
    }

    case TK_VARREF:
    case TK_LETREF:
    {
      ast_error(ast, "not implemented (codegen for local ref)");
      return NULL;
    }

    case TK_IF:
    {
      ast_error(ast, "not implemented (codegen for if)");
      return NULL;
    }

    case TK_WHILE:
    {
      ast_error(ast, "not implemented (codegen for while)");
      return NULL;
    }

    case TK_REPEAT:
    {
      ast_error(ast, "not implemented (codegen for repeat)");
      return NULL;
    }

    case TK_TRY:
    {
      ast_error(ast, "not implemented (codegen for try)");
      return NULL;
    }

    case TK_MATCH:
    {
      ast_error(ast, "not implemented (codegen for match)");
      return NULL;
    }

    case TK_CALL:
    {
      ast_error(ast, "not implemented (codegen for call)");
      return NULL;
    }

    case TK_CONSUME:
    case TK_RECOVER:
      return gen_expr(c, ast_child(ast));

    case TK_BREAK:
    {
      ast_error(ast, "not implemented (codegen for break)");
      return NULL;
    }

    case TK_CONTINUE:
    {
      ast_error(ast, "not implemented (codegen for continue)");
      return NULL;
    }

    case TK_RETURN:
    {
      ast_error(ast, "not implemented (codegen for return)");
      return NULL;
    }

    case TK_ERROR:
    {
      ast_error(ast, "not implemented (codegen for error)");
      return NULL;
    }

    case TK_NOT:
      return gen_not(c, ast);

    case TK_UNARY_MINUS:
      return gen_unaryminus(c, ast);

    case TK_PLUS:
      return gen_plus(c, ast);

    case TK_MINUS:
      return gen_minus(c, ast);

    case TK_MULTIPLY:
      return gen_multiply(c, ast);

    case TK_DIVIDE:
      return gen_divide(c, ast);

    case TK_MOD:
      return gen_mod(c, ast);

    case TK_LSHIFT:
      return gen_lshift(c, ast);

    case TK_RSHIFT:
      return gen_rshift(c, ast);

    case TK_LT:
      return gen_lt(c, ast);

    case TK_LE:
      return gen_le(c, ast);

    case TK_GE:
      return gen_ge(c, ast);

    case TK_GT:
      return gen_gt(c, ast);

    case TK_EQ:
      return gen_eq(c, ast);

    case TK_NE:
      return gen_ne(c, ast);

    case TK_IS:
      return gen_is(c, ast);

    case TK_ISNT:
    {
      ast_error(ast, "not implemented (codegen for isnt)");
      return NULL;
    }

    case TK_AND:
      return gen_and(c, ast);

    case TK_OR:
      return gen_or(c, ast);

    case TK_XOR:
      return gen_xor(c, ast);

    case TK_ASSIGN:
      return gen_assign(c, ast);

    case TK_THIS:
      return gen_this(c, ast);

    case TK_INT:
      return LLVMConstInt(LLVMIntType(128), ast_int(ast), false);

    case TK_FLOAT:
      return LLVMConstReal(LLVMDoubleType(), ast_float(ast));

    case TK_STRING:
    {
      ast_error(ast, "not implemented (codegen for string literal)");
      return NULL;
    }

    case TK_TUPLE:
      return gen_tuple(c, ast);

    default: {}
  }

  ast_error(ast, "not implemented (codegen)");
  return NULL;
}

bool gen_binop(compile_t* c, ast_t* ast,
  LLVMValueRef* l_value, LLVMValueRef* r_value)
{
  ast_t* left;
  ast_t* right;
  AST_GET_CHILDREN(ast, &left, &right);

  *l_value = gen_expr(c, left);
  *r_value = gen_expr(c, right);

  if((*l_value == NULL) || (*r_value == NULL))
    return false;

  return gen_binop_cast(left, right, l_value, r_value);
}

LLVMValueRef gen_literal_cast(LLVMValueRef lit, LLVMValueRef val, bool sign)
{
  LLVMTypeRef type = LLVMTypeOf(val);

  switch(LLVMGetTypeKind(type))
  {
    case LLVMIntegerTypeKind:
      return LLVMConstIntCast(lit, type, sign);

    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      return LLVMConstSIToFP(lit, type);

    default: {}
  }

  assert(0);
  return NULL;
}

bool gen_binop_cast(ast_t* left, ast_t* right, LLVMValueRef* pl_value,
  LLVMValueRef* pr_value)
{
  LLVMValueRef l_value = *pl_value;
  LLVMValueRef r_value = *pr_value;

  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  if(is_intliteral(left_type))
  {
    if(is_floatliteral(right_type))
    {
      *pl_value = LLVMConstSIToFP(l_value, LLVMDoubleType());
      return true;
    }

    *pl_value = gen_literal_cast(l_value, r_value, is_signed(right_type));
    return false;
  } else if(is_intliteral(right_type)) {
    if(is_floatliteral(left_type))
    {
      *pr_value = LLVMConstSIToFP(r_value, LLVMDoubleType());
      return true;
    }

    *pr_value = gen_literal_cast(r_value, l_value, is_signed(left_type));
    return false;
  } else if(is_floatliteral(left_type)) {
    *pl_value = gen_literal_cast(l_value, r_value, true);
    return false;
  } else if(is_floatliteral(right_type)) {
    *pr_value = gen_literal_cast(r_value, l_value, true);
    return false;
  }

  return false;
}

LLVMValueRef gen_assign_cast(compile_t* c, ast_t* left, ast_t* right,
  LLVMValueRef l_value, LLVMValueRef r_value)
{
  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  // the left hand side is a pointer to the actual type we want to cast to
  LLVMTypeRef l_type = LLVMGetElementType(LLVMTypeOf(l_value));
  LLVMTypeRef r_type = LLVMTypeOf(r_value);
  LLVMValueRef r_cast;

  if(is_intliteral(right_type))
  {
    switch(LLVMGetTypeKind(l_type))
    {
      case LLVMIntegerTypeKind:
      {
        r_cast = LLVMConstIntCast(r_value, l_type, is_signed(left_type));
        break;
      }

      case LLVMHalfTypeKind:
      case LLVMFloatTypeKind:
      case LLVMDoubleTypeKind:
        // always treat the literal as signed
        r_cast = LLVMConstSIToFP(r_value, l_type);
        break;

      case LLVMPointerTypeKind:
      {
        // TODO: box the integer literal
        ast_error(right_type, "not implemented (boxing int literals)");
        return NULL;
      }

      default:
        assert(0);
        return NULL;
    }
  } else if(is_floatliteral(right_type)) {
    switch(LLVMGetTypeKind(l_type))
    {
      case LLVMHalfTypeKind:
      case LLVMFloatTypeKind:
      case LLVMDoubleTypeKind:
        r_cast = LLVMConstFPCast(r_value, l_type);
        break;

      case LLVMPointerTypeKind:
      {
        // TODO: box the float literal
        ast_error(right_type, "not implemented (boxing float literals)");
        return NULL;
      }

      default:
        assert(0);
        return NULL;
    }
  } else if(LLVMGetTypeKind(l_type) == LLVMPointerTypeKind) {
    switch(LLVMGetTypeKind(r_type))
    {
      case LLVMHalfTypeKind:
      case LLVMFloatTypeKind:
      case LLVMDoubleTypeKind:
      case LLVMIntegerTypeKind:
      {
        // TODO: box the primitive
        ast_error(right_type, "not implemented (boxing primitives)");
        return NULL;
      }

      case LLVMPointerTypeKind:
        r_cast = LLVMBuildBitCast(c->builder, r_value, l_type, "");
        break;

      default:
        assert(0);
        return NULL;
    }
  } else {
    assert(l_type == r_type);
    r_cast = r_value;
  }

  return r_cast;
}
