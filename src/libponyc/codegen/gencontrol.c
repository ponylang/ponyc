#include "gencontrol.h"
#include "genexpr.h"
#include "gentype.h"
#include "../type/subtype.h"
#include <assert.h>

LLVMValueRef gen_seq(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);
  LLVMValueRef value;

  while(child != NULL)
  {
    value = gen_expr(c, child);

    if(value == NULL)
      return NULL;

    child = ast_sibling(child);
  }

  return value;
}

LLVMValueRef gen_if(compile_t* c, ast_t* ast)
{
  ast_t* cond;
  ast_t* left;
  ast_t* right;
  AST_GET_CHILDREN(ast, &cond, &left, &right);

  ast_t* type = ast_type(ast);
  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);
  bool sign = is_signed(type);

  LLVMTypeRef phi_type = gentype(c, type);

  if(phi_type == NULL)
    return NULL;

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  // build a conditional
  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(fun, "then");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "else");
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(fun, "merge");
  LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  // left branch
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  l_value = gen_assign_cast(c, left_type, phi_type, l_value, sign);
  LLVMBuildBr(c->builder, merge_block);

  // right branch
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  r_value = gen_assign_cast(c, right_type, phi_type, r_value, sign);
  LLVMBuildBr(c->builder, merge_block);

  // continue in the merge block
  LLVMPositionBuilderAtEnd(c->builder, merge_block);

  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");
  LLVMAddIncoming(phi, &l_value, &then_block, 1);
  LLVMAddIncoming(phi, &r_value, &else_block, 1);

  return phi;
}
