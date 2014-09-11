#include "genexpr.h"
#include "genname.h"
#include "genfun.h"
#include "gencontrol.h"
#include "genoperator.h"
#include "genreference.h"
#include "gencall.h"
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
      return gen_localdecl(c, ast);

    case TK_VARREF:
    case TK_LETREF:
      return gen_localload(c, ast);

    case TK_IF:
      return gen_if(c, ast);

    case TK_WHILE:
      return gen_while(c, ast);

    case TK_REPEAT:
      return gen_repeat(c, ast);

    case TK_TRY:
      return gen_try(c, ast);

    case TK_MATCH:
    {
      // TODO: match
      ast_error(ast, "not implemented (codegen for match)");
      return NULL;
    }

    case TK_CALL:
      return gen_call(c, ast);

    case TK_CONSUME:
    case TK_RECOVER:
      return gen_expr(c, ast_child(ast));

    case TK_BREAK:
      return gen_break(c, ast);

    case TK_CONTINUE:
      return gen_continue(c, ast);

    case TK_RETURN:
      return gen_return(c, ast);

    case TK_ERROR:
      return gen_error(c, ast);

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
      return gen_isnt(c, ast);

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
      return gen_string(c, ast);

    case TK_TUPLE:
      return gen_tuple(c, ast);

    case TK_AT:
      return gen_ffi(c, ast);

    case TK_COMPILER_INTRINSIC:
      ast_error(ast, "compiler intrinsic not defined");
      return NULL;

    default: {}
  }

  ast_error(ast, "not implemented (codegen unknown)");
  return NULL;
}

bool gen_binop(compile_t* c, ast_t* ast,
  LLVMValueRef* l_value, LLVMValueRef* r_value)
{
  AST_GET_CHILDREN(ast, left, right);

  *l_value = gen_expr(c, left);
  *r_value = gen_expr(c, right);

  if((*l_value == NULL) || (*r_value == NULL))
    return false;

  return gen_binop_cast(left, right, l_value, r_value);
}

LLVMValueRef gen_literal_cast(LLVMValueRef lit, LLVMValueRef val, bool sign)
{
  LLVMTypeRef from = LLVMTypeOf(lit);
  LLVMTypeRef to = LLVMTypeOf(val);

  switch(LLVMGetTypeKind(from))
  {
    case LLVMIntegerTypeKind:
    {
      switch(LLVMGetTypeKind(to))
      {
        case LLVMIntegerTypeKind:
          return LLVMConstIntCast(lit, to, sign);

        case LLVMHalfTypeKind:
        case LLVMFloatTypeKind:
        case LLVMDoubleTypeKind:
          // TODO: need to know if lit is signed?
          return LLVMConstSIToFP(lit, to);

        default: {}
      }
      break;
    }

    case LLVMHalfTypeKind:
    {
      switch(LLVMGetTypeKind(to))
      {
        case LLVMHalfTypeKind:
          return lit;

        case LLVMFloatTypeKind:
        case LLVMDoubleTypeKind:
          return LLVMConstFPExt(lit, to);

        default: {}
      }
      break;
    }

    case LLVMFloatTypeKind:
    {
      switch(LLVMGetTypeKind(to))
      {
        case LLVMHalfTypeKind:
          return LLVMConstFPTrunc(lit, to);

        case LLVMFloatTypeKind:
          return lit;

        case LLVMDoubleTypeKind:
          return LLVMConstFPExt(lit, to);

        default: {}
      }
      break;
    }

    case LLVMDoubleTypeKind:
    {
      switch(LLVMGetTypeKind(to))
      {
        case LLVMHalfTypeKind:
        case LLVMFloatTypeKind:
          return LLVMConstFPTrunc(lit, to);

        case LLVMDoubleTypeKind:
          return lit;

        default: {}
      }
      break;
    }

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

LLVMValueRef gen_assign_cast(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  if(r_value <= GEN_NOVALUE)
    return r_value;

  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
    {
      switch(LLVMGetTypeKind(r_type))
      {
        case LLVMIntegerTypeKind:
        {
          // integer to integer will be a constant unless they are the same type
          // TODO: check the constant fits in the type
          if(LLVMIsAConstant(r_value))
            return LLVMConstIntCast(r_value, l_type, is_signed(type));

          return r_value;
        }

        default: {}
      }
      break;
    }

    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    {
      switch(LLVMGetTypeKind(r_type))
      {
        case LLVMIntegerTypeKind:
        {
          // integer to float will be a constant
          assert(LLVMIsAConstant(r_value));

          if(is_signed(type))
            r_value = LLVMConstSIToFP(r_value, l_type);
          else
            r_value = LLVMConstUIToFP(r_value, l_type);

          return r_value;
        }

        case LLVMHalfTypeKind:
        case LLVMFloatTypeKind:
        case LLVMDoubleTypeKind:
        {
          // float to float will be a constant unless they are the same type
          if(LLVMIsAConstant(r_value))
            return LLVMConstFPCast(r_value, l_type);

          return r_value;
        }

        default: {}
      }
      break;
    }

    case LLVMPointerTypeKind:
    {
      switch(LLVMGetTypeKind(r_type))
      {
        case LLVMIntegerTypeKind:
        case LLVMHalfTypeKind:
        case LLVMFloatTypeKind:
        case LLVMDoubleTypeKind:
        {
          // Primitive to pointer requires boxing.
          gentype_t g;

          if(!gentype(c, type, &g))
            return NULL;

          LLVMValueRef box_fn = genfun_box(c, &g);
          LLVMValueRef box = LLVMBuildCall(c->builder, box_fn, &r_value, 1, "");
          LLVMSetInstructionCallConv(box, LLVMFastCallConv);

          return LLVMBuildBitCast(c->builder, box, l_type, "");
        }

        case LLVMPointerTypeKind:
          return LLVMBuildBitCast(c->builder, r_value, l_type, "");

        default: {}
      }
      break;
    }

    default: {}
  }

  assert(0);
  return NULL;
}
