#include "genoperator.h"
#include "genexpr.h"
#include "genreference.h"
#include "../type/subtype.h"
#include <assert.h>

static bool is_fp(LLVMValueRef val)
{
  LLVMTypeRef type = LLVMTypeOf(val);

  switch(LLVMGetTypeKind(type))
  {
    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      return true;

    default: {}
  }

  return false;
}

LLVMValueRef gen_lvalue(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
      return gen_localdecl(c, ast);

    case TK_FVARREF:
    case TK_FLETREF:
      return gen_fieldptr(c, ast);

    case TK_VARREF:
      return gen_localptr(c, ast);

    case TK_TUPLE:
      // TODO: tuples as lvalues
      ast_error(ast, "not implemented (lvalue tuple)");
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}

LLVMValueRef gen_not(compile_t* c, ast_t* ast)
{
  LLVMValueRef value = gen_expr(c, ast_child(ast));

  if(value == NULL)
    return NULL;

  if(LLVMIsAConstantInt(value))
    return LLVMConstNot(value);

  return LLVMBuildNot(c->builder, value, "");
}

LLVMValueRef gen_unaryminus(compile_t* c, ast_t* ast)
{
  LLVMValueRef value = gen_expr(c, ast_child(ast));

  if(value == NULL)
    return NULL;

  if(LLVMIsAConstantFP(value))
    return LLVMConstFNeg(value);

  if(LLVMIsAConstantInt(value))
    return LLVMConstNeg(value);

  if(is_fp(value))
    return LLVMBuildFNeg(c->builder, value, "");

  return LLVMBuildNeg(c->builder, value, "");
}

LLVMValueRef gen_plus(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFAdd(l_value, r_value);

    return LLVMConstAdd(l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFAdd(c->builder, l_value, r_value, "");

  return LLVMBuildAdd(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_minus(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFSub(l_value, r_value);

    return LLVMConstSub(l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFSub(c->builder, l_value, r_value, "");

  return LLVMBuildSub(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_multiply(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFMul(l_value, r_value);

    return LLVMConstMul(l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFMul(c->builder, l_value, r_value, "");

  return LLVMBuildMul(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_divide(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(LLVMIsConstant(r_value) && (LLVMConstIntGetSExtValue(r_value) == 0))
  {
    ast_error(ast, "constant divide by zero");
    return NULL;
  }

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFDiv(l_value, r_value);

    if(is_signed(ast_type(ast)))
      return LLVMConstSDiv(l_value, r_value);

    return LLVMConstUDiv(l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFDiv(c->builder, l_value, r_value, "");

  // TODO: check for div by zero
  if(is_signed(ast_type(ast)))
    return LLVMBuildSDiv(c->builder, l_value, r_value, "");

  return LLVMBuildUDiv(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_mod(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(LLVMIsConstant(r_value) && (LLVMConstIntGetSExtValue(r_value) == 0))
  {
    ast_error(ast, "constant modulus zero");
    return NULL;
  }

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFRem(l_value, r_value);

    if(is_signed(ast_type(ast)))
      return LLVMConstSRem(l_value, r_value);

    return LLVMConstURem(l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFRem(c->builder, l_value, r_value, "");

  // TODO: check for div by zero
  if(is_signed(ast_type(ast)))
    return LLVMBuildSRem(c->builder, l_value, r_value, "");

  return LLVMBuildURem(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_lshift(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
    return LLVMConstShl(l_value, r_value);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  return LLVMBuildShl(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_rshift(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);
  ast_t* type = ast_type(ast);

  if(constant)
  {
    if(is_signed(type))
      return LLVMConstAShr(l_value, r_value);

    return LLVMConstLShr(l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_signed(type))
    return LLVMBuildAShr(c->builder, l_value, r_value, "");

  return LLVMBuildLShr(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_lt(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);
  ast_t* type = ast_type(ast);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(LLVMRealOLT, l_value, r_value);

    if(is_signed(type))
      return LLVMConstICmp(LLVMIntSLT, l_value, r_value);

    return LLVMConstICmp(LLVMIntULT, l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, LLVMRealOLT, l_value, r_value, "");

  if(is_signed(type))
    return LLVMBuildICmp(c->builder, LLVMIntSLT, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, LLVMIntULT, l_value, r_value, "");
}

LLVMValueRef gen_le(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);
  ast_t* type = ast_type(ast);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(LLVMRealOLE, l_value, r_value);

    if(is_signed(type))
      return LLVMConstICmp(LLVMIntSLE, l_value, r_value);

    return LLVMConstICmp(LLVMIntULE, l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, LLVMRealOLE, l_value, r_value, "");

  if(is_signed(type))
    return LLVMBuildICmp(c->builder, LLVMIntSLE, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, LLVMIntULE, l_value, r_value, "");
}

LLVMValueRef gen_ge(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);
  ast_t* type = ast_type(ast);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(LLVMRealOGE, l_value, r_value);

    if(is_signed(type))
      return LLVMConstICmp(LLVMIntSGE, l_value, r_value);

    return LLVMConstICmp(LLVMIntUGE, l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, LLVMRealOGE, l_value, r_value, "");

  if(is_signed(type))
    return LLVMBuildICmp(c->builder, LLVMIntSGE, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, LLVMIntUGE, l_value, r_value, "");
}

LLVMValueRef gen_gt(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);
  ast_t* type = ast_type(ast);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(LLVMRealOGT, l_value, r_value);

    if(is_signed(type))
      return LLVMConstICmp(LLVMIntSGT, l_value, r_value);

    return LLVMConstICmp(LLVMIntUGT, l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, LLVMRealOGT, l_value, r_value, "");

  if(is_signed(type))
    return LLVMBuildICmp(c->builder, LLVMIntSGT, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, LLVMIntUGT, l_value, r_value, "");
}

LLVMValueRef gen_eq(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(LLVMRealOEQ, l_value, r_value);

    return LLVMConstICmp(LLVMIntEQ, l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, LLVMRealOEQ, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");
}

LLVMValueRef gen_ne(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(LLVMRealONE, l_value, r_value);

    return LLVMConstICmp(LLVMIntNE, l_value, r_value);
  }

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, LLVMRealONE, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, LLVMIntNE, l_value, r_value, "");
}

LLVMValueRef gen_is(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMTypeRef left_type = LLVMTypeOf(l_value);
  LLVMTypeRef right_type = LLVMTypeOf(r_value);

  if(LLVMGetTypeKind(left_type) != LLVMPointerTypeKind)
  {
    // TODO: match primitive left with object right
    ast_error(left, "not implemented (codegen identity with primitive)");
    return NULL;
  }

  if(LLVMGetTypeKind(right_type) != LLVMPointerTypeKind)
  {
    // TODO: match object left with primitive right
    ast_error(right, "not implemented (codegen identity with primitive)");
    return NULL;
  }

  // TODO: structural check if both sides are the same boxed primitive type
  LLVMTypeRef type = LLVMIntPtrType(c->target);
  l_value = LLVMBuildPtrToInt(c->builder, l_value, type, "");
  r_value = LLVMBuildPtrToInt(c->builder, r_value, type, "");

  return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");
}

LLVMValueRef gen_isnt(compile_t* c, ast_t* ast)
{
  LLVMValueRef is = gen_is(c, ast);

  if(is == NULL)
    return NULL;

  return LLVMBuildNot(c->builder, is, "");
}

LLVMValueRef gen_and(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
    return LLVMConstAnd(l_value, r_value);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  return LLVMBuildAnd(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_or(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
    return LLVMConstOr(l_value, r_value);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  return LLVMBuildOr(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_xor(compile_t* c, ast_t* ast)
{
  LLVMValueRef l_value;
  LLVMValueRef r_value;
  bool constant = gen_binop(c, ast, &l_value, &r_value);

  if(constant)
    return LLVMConstXor(l_value, r_value);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  return LLVMBuildXor(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_assign(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  LLVMValueRef l_value = gen_lvalue(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  bool l_sign = is_signed(left_type);
  bool r_sign = is_signed(right_type);
  LLVMTypeRef l_type = LLVMGetElementType(LLVMTypeOf(l_value));
  r_value = gen_assign_cast(c, l_type, r_value, l_sign, r_sign);

  if(r_value == NULL)
    return NULL;

  // If the l_value is a local declaration, the result is be the r_value, not
  // the load of the l_value.
  LLVMValueRef result;

  if(LLVMIsAAllocaInst(l_value))
    result = r_value;
  else
    result = LLVMBuildLoad(c->builder, l_value, "");

  LLVMBuildStore(c->builder, r_value, l_value);
  return result;
}
