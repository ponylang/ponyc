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

  bool sign = is_signed(type);
  bool l_sign = is_signed(left_type);
  bool r_sign = is_signed(right_type);

  LLVMTypeRef phi_type;

  // we will have no type if both branches have return statements
  if(type != NULL)
  {
    phi_type = gentype(c, type);

    if(phi_type == NULL)
      return NULL;
  }

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  // build a conditional
  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(fun, "if_then");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "if_else");
  LLVMBasicBlockRef post_block;

  // if both branches return, we have no post block
  if(type != NULL)
    post_block = LLVMAppendBasicBlock(fun, "if_post");

  LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  // left branch
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  if(l_value != GEN_NOVALUE)
  {
    l_value = gen_assign_cast(c, phi_type, l_value, sign, l_sign);
    LLVMBuildBr(c->builder, post_block);
  }

  // right branch
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  // if the right side returns, we don't branch to the post block
  if(r_value != GEN_NOVALUE)
  {
    r_value = gen_assign_cast(c, phi_type, r_value, sign, r_sign);
    LLVMBuildBr(c->builder, post_block);
  }

  // if both sides return, we return a sentinal value
  if(type == NULL)
    return GEN_NOVALUE;

  // continue in the post block
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");

  if(l_value != GEN_NOVALUE)
    LLVMAddIncoming(phi, &l_value, &then_block, 1);

  if(r_value != GEN_NOVALUE)
    LLVMAddIncoming(phi, &r_value, &else_block, 1);

  return phi;
}

LLVMValueRef gen_while(compile_t* c, ast_t* ast)
{
  ast_t* cond;
  ast_t* body;
  ast_t* else_clause;
  AST_GET_CHILDREN(ast, &cond, &body, &else_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  bool sign = is_signed(type);
  bool body_sign = is_signed(body_type);
  bool else_sign = is_signed(else_type);
  LLVMTypeRef phi_type;

  // we will have no type if the body returns
  if(type != NULL)
  {
    phi_type = gentype(c, type);

    if(phi_type == NULL)
      return NULL;
  }

  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(fun, "while_cond");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(fun, "while_body");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "while_else");
  LLVMBasicBlockRef post_block;

  if(type != NULL)
    post_block = LLVMAppendBasicBlock(fun, "while_post");

  // entry
  LLVMValueRef e_value = gen_expr(c, cond);

  if(e_value == NULL)
    return NULL;

  LLVMBuildCondBr(c->builder, e_value, body_block, else_block);

  // condition
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  LLVMBuildCondBr(c->builder, c_value, body_block, post_block);

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef l_value = gen_expr(c, body);

  if(l_value == NULL)
    return NULL;

  // if the body doesn't return, cast to the phi type
  if(l_value != GEN_NOVALUE)
  {
    l_value = gen_assign_cast(c, phi_type, l_value, sign, body_sign);
    LLVMBuildBr(c->builder, cond_block);
  }

  // else
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, else_clause);

  if(r_value == NULL)
    return NULL;

  // if the else clause doesn't return, cast to the phi type
  if(r_value != GEN_NOVALUE)
  {
    r_value = gen_assign_cast(c, phi_type, r_value, sign, else_sign);
    LLVMBuildBr(c->builder, post_block);
  }

  // if both sides return, we return a sentinal value
  if(type == NULL)
    return GEN_NOVALUE;

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");
  LLVMAddIncoming(phi, &l_value, &cond_block, 1);
  LLVMAddIncoming(phi, &r_value, &else_block, 1);

  return phi;
}

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast)
{
  ast_t* body;
  ast_t* cond;
  AST_GET_CHILDREN(ast, &body, &cond);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);

  bool sign = is_signed(type);
  bool body_sign = is_signed(body_type);
  LLVMTypeRef phi_type;

  // we will have no type if the body returns
  if(type != NULL)
  {
    phi_type = gentype(c, type);

    if(phi_type == NULL)
      return NULL;
  }

  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(fun, "repeat_body");
  LLVMBasicBlockRef post_block;

  // if the body returns, we have no post block
  if(type != NULL)
    post_block = LLVMAppendBasicBlock(fun, "repeat_post");

  LLVMBuildBr(c->builder, body_block);

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef value = gen_expr(c, body);

  if(value == NULL)
    return NULL;

  // if the body doesn't return, cast to the phi type
  if(value != GEN_NOVALUE)
  {
    value = gen_assign_cast(c, phi_type, value, sign, body_sign);
    LLVMValueRef c_value = gen_expr(c, cond);
    LLVMBuildCondBr(c->builder, c_value, body_block, post_block);
  }

  if(type == NULL)
    return GEN_NOVALUE;

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");
  LLVMAddIncoming(phi, &value, &body_block, 1);

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
    LLVMBuildRetVoid(c->builder);
    return GEN_NOVALUE;
  }

  LLVMBuildRet(c->builder, value);
  return GEN_NOVALUE;
}
