#include "gencontrol.h"
#include "genexpr.h"
#include "gentype.h"
#include "gencall.h"
#include "genname.h"
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
  AST_GET_CHILDREN(ast, cond, left, right);

  ast_t* type = ast_type(ast);
  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  gentype_t phi_type;

  // we will have no type if both branches have return statements
  if((type != NULL) && !gentype(c, type, &phi_type))
    return NULL;

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  // build a conditional
  LLVMValueRef fun = codegen_fun(c);
  LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(c->context,
    fun, "if_then");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlockInContext(c->context,
    fun, "if_else");
  LLVMBasicBlockRef post_block = NULL;

  // if both branches return, we have no post block
  if(type != NULL)
    post_block = LLVMAppendBasicBlockInContext(c->context, fun, "if_post");

  LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  // left branch
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value != GEN_NOVALUE)
  {
    l_value = gen_assign_cast(c, phi_type.use_type, l_value, left_type);

    if(l_value == NULL)
      return NULL;

    then_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // right branch
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, right);

  // if the right side returns, we don't branch to the post block
  if(r_value != GEN_NOVALUE)
  {
    r_value = gen_assign_cast(c, phi_type.use_type, r_value, right_type);

    if(r_value == NULL)
      return NULL;

    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // if both sides return, we return a sentinal value
  if(type == NULL)
    return GEN_NOVALUE;

  // continue in the post block
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

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

  gentype_t phi_type;

  if(!gentype(c, type, &phi_type))
    return NULL;

  LLVMBasicBlockRef init_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "while_init");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "while_body");
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "while_else");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "while_post");
  LLVMBuildBr(c->builder, init_block);

  // keep a reference to the init block
  ast_setdata(ast, init_block);

  // start the post block so that a break can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // init
  LLVMPositionBuilderAtEnd(c->builder, init_block);
  LLVMValueRef i_value = gen_expr(c, cond);

  if(i_value == NULL)
    return NULL;

  LLVMBuildCondBr(c->builder, i_value, body_block, else_block);

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef l_value = gen_expr(c, body);
  l_value = gen_assign_cast(c, phi_type.use_type, l_value, body_type);

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

  r_value = gen_assign_cast(c, phi_type.use_type, r_value, else_type);
  LLVMBasicBlockRef else_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMAddIncoming(phi, &l_value, &body_from, 1);

  if(r_value != GEN_NOVALUE)
    LLVMAddIncoming(phi, &r_value, &else_from, 1);

  return phi;
}

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, cond);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);

  gentype_t phi_type;

  if(!gentype(c, type, &phi_type))
    return NULL;

  LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "repeat_body");
  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "repeat_cond");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "repeat_post");
  LLVMBuildBr(c->builder, body_block);

  // keep a reference to the cond block
  ast_setdata(ast, cond_block);

  // start the cond block so that a continue can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef cond_phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // start the post block so that a break can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef value = gen_expr(c, body);
  value = gen_assign_cast(c, phi_type.use_type, value, body_type);

  if(value == NULL)
    return NULL;

  body_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, cond_block);

  // cond block
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMAddIncoming(cond_phi, &value, &body_block, 1);

  LLVMValueRef c_value = gen_expr(c, cond);
  cond_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildCondBr(c->builder, c_value, post_block, body_block);

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMAddIncoming(phi, &cond_phi, &cond_block, 1);

  return phi;
}

LLVMValueRef gen_break(compile_t* c, ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);
  LLVMBasicBlockRef target = (LLVMBasicBlockRef)ast_data(loop);

  ast_t* body = ast_child(ast);
  ast_t* body_type = ast_type(body);

  LLVMBasicBlockRef break_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "break");
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
  value = gen_assign_cast(c, phi_type, value, body_type);

  if(value == NULL)
    return NULL;

  break_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, target);

  // add break value to the post block phi node
  LLVMAddIncoming(post_phi, &value, &break_block, 1);
  return GEN_NOVALUE;
}

LLVMValueRef gen_continue(compile_t* c, ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);
  LLVMBasicBlockRef target = (LLVMBasicBlockRef)ast_data(loop);

  switch(ast_id(loop))
  {
    case TK_REPEAT:
    {
      // create a new continue block before the condition block
      LLVMBasicBlockRef cont_block = LLVMInsertBasicBlockInContext(c->context,
        target, "continue");

      // jump to the continue block
      LLVMBuildBr(c->builder, cont_block);

      // jump with none to the condition block
      LLVMPositionBuilderAtEnd(c->builder, cont_block);
      const char* inst = genname_instance("$1_None");
      LLVMValueRef none = LLVMGetNamedGlobal(c->module, inst);
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
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);

  size_t clause;
  ast_t* try_expr = ast_enclosing_try(ast, &clause);

  // do the then block only if we return in the body or else clause
  // in the then block, return without doing the then block
  if((try_expr != NULL) && (clause != 2))
    gen_expr(c, ast_childidx(try_expr, 2));

  LLVMBuildRet(c->builder, value);
  return GEN_NOVALUE;
}

LLVMValueRef gen_try(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  gentype_t phi_type;

  // we will have no type if both branches have return statements
  if((type != NULL) && !gentype(c, type, &phi_type))
    return NULL;

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef else_block = LLVMAppendBasicBlockInContext(c->context,
    codegen_fun(c), "try_else");
  LLVMBasicBlockRef post_block = NULL;

  if(type != NULL)
  {
    post_block = LLVMAppendBasicBlockInContext(c->context, codegen_fun(c),
      "try_post");
  }

  // keep a reference to the else block
  ast_setdata(ast, else_block);

  // body block
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef body_value = gen_expr(c, body);

  if(body_value != GEN_NOVALUE)
  {
    body_value = gen_assign_cast(c, phi_type.use_type, body_value, body_type);

    if(body_value == NULL)
      return NULL;

    gen_expr(c, then_clause);
    block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // else block
  LLVMPositionBuilderAtEnd(c->builder, else_block);

  // the landing pad is marked as a cleanup, since exceptions are typeless and
  // valueless. the first landing pad is always the destination.
  LLVMTypeRef lp_elements[2];
  lp_elements[0] = c->void_ptr;
  lp_elements[1] = c->i32;
  LLVMTypeRef lp_type = LLVMStructTypeInContext(c->context, lp_elements, 2,
    false);
  LLVMValueRef landing = LLVMBuildLandingPad(c->builder, lp_type,
    c->personality, 0, "");
  LLVMSetCleanup(landing, true);

  LLVMValueRef else_value = gen_expr(c, else_clause);

  if(else_value != GEN_NOVALUE)
  {
    else_value = gen_assign_cast(c, phi_type.use_type, else_value, else_type);

    if(else_value == NULL)
      return NULL;

    gen_expr(c, then_clause);
    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // if both sides return, we return a sentinal value
  if(type == NULL)
    return GEN_NOVALUE;

  // continue in the post block
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  if(body_value != GEN_NOVALUE)
    LLVMAddIncoming(phi, &body_value, &block, 1);

  if(else_value != GEN_NOVALUE)
    LLVMAddIncoming(phi, &else_value, &else_block, 1);

  return phi;
}

LLVMValueRef gen_error(compile_t* c, ast_t* ast)
{
  size_t clause;
  ast_t* try_expr = ast_enclosing_try(ast, &clause);

  // do the then block only if we error out in the else clause
  // in the body or the then block, error out without doing the then block
  if(try_expr != NULL)
  {
    switch(clause)
    {
      case 0:
      {
        // We're in the body. Invoke throw rather than calling it, so that
        // we can give it a landing pad.
        gencall_throw(c, try_expr);
        return GEN_NOVALUE;
      }

      case 1:
      {
        // We're in the else clause. Do the then clause and throw.
        gen_expr(c, ast_childidx(try_expr, 2));
        break;
      }

      case 2:
      {
        // We're in the then clause. Just need to throw.
        break;
      }

      default:
      {
        assert(0);
        return NULL;
      }
    }
  }

  // Actually throw.
  gencall_throw(c, NULL);
  return GEN_NOVALUE;
}
