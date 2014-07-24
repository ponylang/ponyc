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
      ast_error(ast, "not implemented (create local)");
      return NULL;

    case TK_FVARREF:
    case TK_FLETREF:
      return gen_fieldptr(c, ast);

    case TK_VARREF:
    case TK_PARAMREF:
      ast_error(ast, "not implemented (lvalue local)");
      return NULL;

    case TK_TUPLE:
      ast_error(ast, "not implemented (lvalue tuple)");
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
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

  if(constant)
  {
    if(is_fp(l_value))
      return LLVMConstFDiv(l_value, r_value);

    // TODO: check for div by zero
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

LLVMValueRef gen_assign(compile_t* c, ast_t* ast)
{
  ast_t* left;
  ast_t* right;
  AST_GET_CHILDREN(ast, &left, &right);

  LLVMValueRef l_value = gen_lvalue(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result = LLVMBuildLoad(c->builder, l_value, "");
  LLVMValueRef r_cast = gen_assign_cast(c, left, right, l_value, r_value);

  if(r_cast == NULL)
    return NULL;

  LLVMBuildStore(c->builder, r_cast, l_value);
  return result;
}
