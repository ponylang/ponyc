#include "gencontrol.h"
#include "genexpr.h"
#include "gentype.h"
#include "../type/subtype.h"
#include <assert.h>

LLVMValueRef gen_seq(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);
  LLVMValueRef value = NULL;

  while(child != NULL)
  {
    if(ast_id(child) == TK_NONE)
      break;

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

  bool sign;
  LLVMTypeRef phi_type;

  if(type != NULL)
  {
    sign = is_signed(type);
    phi_type = gentype(c, type);

    if(phi_type == NULL)
      return NULL;
  }

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  // build a conditional
  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(fun, "then");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "else");
  LLVMBasicBlockRef merge_block;

  if(type != NULL)
    merge_block = LLVMAppendBasicBlock(fun, "merge");

  LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  // left branch
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  if(left_type != NULL)
  {
    l_value = gen_assign_cast(c, left_type, phi_type, l_value, sign);
    LLVMBuildBr(c->builder, merge_block);
  }

  // right branch
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  if(right_type != NULL)
  {
    r_value = gen_assign_cast(c, right_type, phi_type, r_value, sign);
    LLVMBuildBr(c->builder, merge_block);
  }

  if(type == NULL)
    return ((LLVMValueRef)1);

  // continue in the merge block
  LLVMPositionBuilderAtEnd(c->builder, merge_block);

  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");

  if(left_type != NULL)
    LLVMAddIncoming(phi, &l_value, &then_block, 1);

  if(right_type != NULL)
    LLVMAddIncoming(phi, &r_value, &else_block, 1);

  return phi;
}

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast)
{
  ast_t* body;
  ast_t* cond;
  AST_GET_CHILDREN(ast, &body, &cond);

  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef loop_block = LLVMAppendBasicBlock(fun, "loop");
  LLVMBasicBlockRef postloop_block = LLVMAppendBasicBlock(fun, "postloop");
  LLVMBuildBr(c->builder, loop_block);

  LLVMPositionBuilderAtEnd(c->builder, loop_block);
  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
    return NULL;

  LLVMValueRef c_value = gen_expr(c, cond);
  LLVMBuildCondBr(c->builder, c_value, loop_block, postloop_block);
  LLVMPositionBuilderAtEnd(c->builder, postloop_block);

  LLVMValueRef phi = LLVMBuildPhi(c->builder, LLVMTypeOf(value), "");
  LLVMAddIncoming(phi, &value, &loop_block, 1);

  return phi;
}

LLVMValueRef gen_return(compile_t* c, ast_t* ast)
{
  ast_t* fun = ast_enclosing_method(ast);
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);

  if((fun != NULL) && (ast_id(fun) == TK_BE))
  {
    // return in a behaviour
    return LLVMBuildRetVoid(c->builder);
  }

  return LLVMBuildRet(c->builder, value);
}
