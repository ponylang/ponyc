#include "genoperator.h"
#include "genexpr.h"
#include "genreference.h"
#include "../type/subtype.h"
#include <assert.h>

static LLVMValueRef assign_rvalue(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value);

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

static LLVMValueRef assign_one(compile_t* c, LLVMValueRef l_value,
  LLVMValueRef r_value, ast_t* r_type)
{
  LLVMValueRef result = LLVMBuildLoad(c->builder, l_value, "");

  // Cast the rvalue appropriately.
  LLVMTypeRef l_type = LLVMGetElementType(LLVMTypeOf(l_value));
  LLVMValueRef cast_value = gen_assign_cast(c, l_type, r_value, r_type);

  if(cast_value == NULL)
    return NULL;

  // Store to the field.
  LLVMBuildStore(c->builder, cast_value, l_value);
  return result;
}

static bool assign_tuple(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  // Handle assignment for each component.
  ast_t* child = ast_child(left);
  ast_t* rtype_child = ast_child(r_type);
  int i = 0;

  while(child != NULL)
  {
    // The actual tuple expression is inside a sequence node.
    ast_t* expr = ast_child(child);

    // Extract the tuple value.
    LLVMValueRef rvalue_child = LLVMBuildExtractValue(c->builder, r_value,
      i++, "");

    // Recurse, assigning pairwise.
    if(assign_rvalue(c, expr, rtype_child, rvalue_child) == NULL)
      return false;

    child = ast_sibling(child);
    rtype_child = ast_sibling(rtype_child);
  }

  assert(rtype_child == NULL);
  return true;
}

static bool assign_decl(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  ast_t* idseq = ast_child(left);
  ast_t* id = ast_child(idseq);

  if(ast_sibling(id) == NULL)
    return assign_rvalue(c, id, r_type, r_value) != NULL;

  ast_t* rtype_child = ast_child(r_type);
  int i = 0;

  while(id != NULL)
  {
    // Extract the tuple value.
    LLVMValueRef rvalue_child = LLVMBuildExtractValue(c->builder, r_value,
      i++, "");

    // Recurse, assigning pairwise.
    if(assign_rvalue(c, id, rtype_child, rvalue_child) == NULL)
      return false;

    id = ast_sibling(id);
    rtype_child = ast_sibling(rtype_child);
  }

  assert(rtype_child == NULL);
  return true;
}

static LLVMValueRef assign_rvalue(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  switch(ast_id(left))
  {
    case TK_VAR:
    case TK_LET:
    {
      // TODO: this should be true any time the l_value is uninitialised.
      // Generate the locals.
      if(gen_localdecl(c, left) == NULL)
        return NULL;

      // Treat as a tuple assignment.
      if(!assign_decl(c, left, r_type, r_value))
        return NULL;

      // If the l_value is a local declaration, the result is the r_value.
      return r_value;
    }

    case TK_FVARREF:
    case TK_FLETREF:
    {
      // The result is the previous value of the field.
      LLVMValueRef l_value = gen_fieldptr(c, left);
      return assign_one(c, l_value, r_value, r_type);
    }

    case TK_VARREF:
    {
      // The result is the previous value of the local.
      LLVMValueRef l_value = gen_localptr(c, left);
      return assign_one(c, l_value, r_value, r_type);
    }

    case TK_TUPLE:
    {
      // If the l_value is a tuple, assemble it as the result.
      LLVMValueRef result = gen_expr(c, left);

      if(result == NULL)
        return NULL;

      if(!assign_tuple(c, left, r_type, r_value))
        return NULL;

      // Return the original tuple.
      return result;
    }

    case TK_ID:
    {
      // We have recursed here from a VAR or LET.
      const char* name = ast_name(left);
      ast_t* def = ast_get(left, name);

      LLVMValueRef l_value = (LLVMValueRef)ast_data(def);
      return assign_one(c, l_value, r_value, r_type);
    }

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

  // Setup additional blocks.
  LLVMBasicBlockRef insert = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "div_then");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "div_post");

  // Check for div by zero.
  LLVMTypeRef type = LLVMTypeOf(r_value);
  LLVMValueRef zero = LLVMConstInt(type, 0, false);
  LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntNE, r_value, zero, "");
  LLVMBuildCondBr(c->builder, cmp, then_block, post_block);

  // Divisor is not zero.
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef result;

  if(is_signed(ast_type(ast)))
    result = LLVMBuildSDiv(c->builder, l_value, r_value, "");
  else
    result = LLVMBuildUDiv(c->builder, l_value, r_value, "");

  LLVMBuildBr(c->builder, post_block);

  // Phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, type, "");
  LLVMAddIncoming(phi, &zero, &insert, 1);
  LLVMAddIncoming(phi, &result, &then_block, 1);

  return phi;
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

  // Setup additional blocks.
  LLVMBasicBlockRef insert = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "mod_then");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "mod_post");

  // Check for mod by zero.
  LLVMTypeRef type = LLVMTypeOf(r_value);
  LLVMValueRef zero = LLVMConstInt(type, 0, false);
  LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntNE, r_value, zero, "");
  LLVMBuildCondBr(c->builder, cmp, then_block, post_block);

  // Divisor is not zero.
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef result;

  if(is_signed(ast_type(ast)))
    result = LLVMBuildSRem(c->builder, l_value, r_value, "");
  else
    result = LLVMBuildURem(c->builder, l_value, r_value, "");

  LLVMBuildBr(c->builder, post_block);

  // Phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, type, "");
  LLVMAddIncoming(phi, &zero, &insert, 1);
  LLVMAddIncoming(phi, &result, &then_block, 1);

  return phi;
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
  LLVMTypeRef type = LLVMIntPtrTypeInContext(c->context, c->target_data);
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
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  return assign_rvalue(c, left, ast_type(right), r_value);
}
