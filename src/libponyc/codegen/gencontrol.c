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
  AST_GET_CHILDREN(ast, cond, left, right);

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

  if(l_value != GEN_NOVALUE)
  {
    l_value = gen_assign_cast(c, phi_type, l_value, sign, l_sign);

    if(l_value == NULL)
      return NULL;

    LLVMBuildBr(c->builder, post_block);
  }

  // right branch
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, right);

  // if the right side returns, we don't branch to the post block
  if(r_value != GEN_NOVALUE)
  {
    r_value = gen_assign_cast(c, phi_type, r_value, sign, r_sign);

    if(r_value == NULL)
      return NULL;

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
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  bool sign = is_signed(type);
  bool body_sign = is_signed(body_type);
  bool else_sign = is_signed(else_type);
  LLVMTypeRef phi_type = gentype(c, type);

  if(phi_type == NULL)
    return NULL;

  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef init_block = LLVMAppendBasicBlock(fun, "while_init");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(fun, "while_body");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(fun, "while_else");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlock(fun, "while_post");
  LLVMBuildBr(c->builder, init_block);

  // keep a reference to the init block
  ast_setdata(ast, init_block);

  // start the post block so that a break can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");

  // init
  LLVMPositionBuilderAtEnd(c->builder, init_block);
  LLVMValueRef i_value = gen_expr(c, cond);

  if(i_value == NULL)
    return NULL;

  LLVMBuildCondBr(c->builder, i_value, body_block, else_block);

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef l_value = gen_expr(c, body);
  l_value = gen_assign_cast(c, phi_type, l_value, sign, body_sign);

  if(l_value == NULL)
    return NULL;

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  LLVMBasicBlockRef body_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildCondBr(c->builder, c_value, body_block, post_block);

  // else
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, else_clause);

  if(r_value == NULL)
    return NULL;

  r_value = gen_assign_cast(c, phi_type, r_value, sign, else_sign);
  LLVMBasicBlockRef else_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMAddIncoming(phi, &l_value, &body_from, 1);
  LLVMAddIncoming(phi, &r_value, &else_from, 1);

  return phi;
}

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, cond);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);

  bool sign = is_signed(type);
  bool body_sign = is_signed(body_type);
  LLVMTypeRef phi_type = gentype(c, type);

  if(phi_type == NULL)
    return NULL;

  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(fun, "repeat_body");
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(fun, "repeat_cond");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlock(fun, "repeat_post");
  LLVMBuildBr(c->builder, body_block);

  // keep a reference to the cond block
  ast_setdata(ast, cond_block);

  // start the cond block so that a continue can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef cond_phi = LLVMBuildPhi(c->builder, phi_type, "");

  // start the post block so that a break can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type, "");

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef value = gen_expr(c, body);
  value = gen_assign_cast(c, phi_type, value, sign, body_sign);

  if(value == NULL)
    return NULL;

  LLVMBuildBr(c->builder, cond_block);

  // cond block
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMAddIncoming(cond_phi, &value, &body_block, 1);

  LLVMValueRef c_value = gen_expr(c, cond);
  LLVMBasicBlockRef cond_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildCondBr(c->builder, c_value, post_block, body_block);

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMAddIncoming(phi, &cond_phi, &cond_from, 1);

  return phi;
}

LLVMValueRef gen_break(compile_t* c, ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);
  ast_t* type = ast_type(loop);
  bool sign = is_signed(type);
  LLVMBasicBlockRef target = ast_data(loop);

  ast_t* body = ast_child(ast);
  ast_t* body_type = ast_type(body);
  bool body_sign = is_signed(body_type);

  LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInsertBlock(c->builder));
  LLVMBasicBlockRef break_block = LLVMAppendBasicBlock(fun, "break");
  LLVMBuildBr(c->builder, break_block);

  switch(ast_id(loop))
  {
    case TK_REPEAT:
    {
      // target is cond_block, need to get to the post_block
      target = LLVMGetNextBasicBlock(target); // post
      break;
    }

    case TK_WHILE:
    {
      // target is init_block, need to get to the post_block
      target = LLVMGetNextBasicBlock(target); // body
      target = LLVMGetNextBasicBlock(target); // else
      target = LLVMGetNextBasicBlock(target); // post
      break;
    }

    default:
      assert(0);
      return NULL;
  }

  // get the phi node
  LLVMValueRef post_phi = LLVMGetFirstInstruction(target);
  LLVMTypeRef phi_type = LLVMTypeOf(post_phi);

  // build the break block
  LLVMPositionBuilderAtEnd(c->builder, break_block);
  LLVMValueRef value = gen_expr(c, body);
  value = gen_assign_cast(c, phi_type, value, sign, body_sign);

  if(value == NULL)
    return NULL;

  LLVMBuildBr(c->builder, target);

  // add break value to the post block phi node
  LLVMAddIncoming(post_phi, &value, &break_block, 1);
  return GEN_NOVALUE;
}

LLVMValueRef gen_continue(compile_t* c, ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);
  LLVMBasicBlockRef target = ast_data(loop);

  switch(ast_id(loop))
  {
    case TK_REPEAT:
    {
      // create a new continue block before the condition block
      LLVMBasicBlockRef cont_block = LLVMInsertBasicBlock(target, "continue");

      // jump to the continue block
      LLVMBuildBr(c->builder, cont_block);

      // jump with none to the condition block
      LLVMPositionBuilderAtEnd(c->builder, cont_block);
      LLVMValueRef none = LLVMGetNamedGlobal(c->module, "$1_$None_$inst");
      LLVMBuildBr(c->builder, target);

      // add none from the continue block to the condition block phi node
      LLVMValueRef cond_phi = LLVMGetFirstInstruction(target);
      LLVMAddIncoming(cond_phi, &none, &cont_block, 1);

      return GEN_NOVALUE;
    }

    case TK_WHILE:
    {
      // jump to the init block
      LLVMBuildBr(c->builder, target);
      return GEN_NOVALUE;
    }

    default: {}
  }

  assert(0);
  return NULL;
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

LLVMValueRef gen_error(compile_t* c, ast_t* ast)
{
  LLVMValueRef zero = LLVMConstInt(LLVMInt1Type(), 0, false);
  LLVMBuildResume(c->builder, zero);
  return GEN_NOVALUE;
}
