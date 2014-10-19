#include "gencontrol.h"
#include "genexpr.h"
#include "gentype.h"
#include "gencall.h"
#include "genname.h"
#include "../type/subtype.h"
#include <assert.h>

static bool is_result_needed(ast_t* ast)
{
  // If we're not in a sequence, then we need the result.
  ast_t* seq = ast_parent(ast);

  if(ast_id(seq) != TK_SEQ)
    return true;

  // If we are not the last element of the sequence, we don't need the result.
  if(ast_sibling(ast) != NULL)
    return false;

  ast_t* parent = ast_parent(seq);

  switch(ast_id(parent))
  {
    case TK_IF:
    case TK_WHILE:
    case TK_MATCH:
      // Condition needed, body/else needed only if parent needed.
      if(ast_child(parent) == seq)
        return true;

      return is_result_needed(parent);

    case TK_REPEAT:
      // Cond needed, body/else needed only if parent needed.
      if(ast_childidx(parent, 1) == seq)
        return true;

      return is_result_needed(parent);

    case TK_CASE:
      // Pattern, guard needed, body needed only if MATCH needed
      if(ast_childidx(parent, 2) != seq)
        return true;

      return is_result_needed(ast_parent(parent));

    case TK_TRY:
      // Only if parent needed.
      return is_result_needed(parent);

    default: {}
  }

  // All others needed.
  return true;
}

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
  bool needed = is_result_needed(ast);
  ast_t* type = ast_type(ast);
  AST_GET_CHILDREN(ast, cond, left, right);

  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  gentype_t phi_type;

  // We will have no type if both branches have return statements.
  if((type != NULL) && !gentype(c, type, &phi_type))
    return NULL;

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  // If the conditional is constant, generate only one branch.
  if(LLVMIsAConstantInt(c_value))
  {
    int value = (int)LLVMConstIntGetZExtValue(c_value);

    if(value == 0)
      return gen_expr(c, right);

    return gen_expr(c, left);
  }

  // build a conditional
  LLVMBasicBlockRef then_block = codegen_block(c, "if_then");
  LLVMBasicBlockRef else_block = codegen_block(c, "if_else");
  LLVMBasicBlockRef post_block = NULL;

  // If both branches return, we have no post block.
  if(type != NULL)
    post_block = codegen_block(c, "if_post");

  LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  // Left branch.
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value != GEN_NOVALUE)
  {
    if(needed)
      l_value = gen_assign_cast(c, phi_type.use_type, l_value, left_type);

    if(l_value == NULL)
      return NULL;

    then_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // Right branch.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, right);

  // If the right side returns, we don't branch to the post block.
  if(r_value != GEN_NOVALUE)
  {
    if(needed)
      r_value = gen_assign_cast(c, phi_type.use_type, r_value, right_type);

    if(r_value == NULL)
      return NULL;

    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // If both sides return, we return a sentinal value.
  if(type == NULL)
    return GEN_NOVALUE;

  // Continue in the post block.
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

    if(l_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &l_value, &then_block, 1);

    if(r_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &r_value, &else_block, 1);

    return phi;
  }

  return GEN_NOVALUE;
}

LLVMValueRef gen_while(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  gentype_t phi_type;

  if(!gentype(c, type, &phi_type))
    return NULL;

  LLVMBasicBlockRef init_block = codegen_block(c, "while_init");
  LLVMBasicBlockRef body_block = codegen_block(c, "while_body");
  LLVMBasicBlockRef else_block = codegen_block(c, "while_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "while_post");
  LLVMBuildBr(c->builder, init_block);

  // keep a reference to the init block
  ast_setdata(ast, init_block);

  // start the post block so that a break can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi;

  if(needed)
    phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // init
  // This jumps either to the body or the else clause. This is not evaluated
  // on each loop iteration: only on the first entry or after a continue.
  LLVMPositionBuilderAtEnd(c->builder, init_block);
  LLVMValueRef i_value = gen_expr(c, cond);

  if(i_value == NULL)
    return NULL;

  LLVMBuildCondBr(c->builder, i_value, body_block, else_block);

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef l_value = gen_expr(c, body);

  if(needed)
    l_value = gen_assign_cast(c, phi_type.use_type, l_value, body_type);

  if(l_value == NULL)
    return NULL;

  // The body evaluates the condition itself, jumping either back to the body
  // or directly to the post block.
  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  LLVMBasicBlockRef body_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildCondBr(c->builder, c_value, body_block, post_block);

  // else
  // If the loop doesn't generate a value (doesn't execute, or continues on the
  // last iteration), the else clause generates the value.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, else_clause);

  if(r_value == NULL)
    return NULL;

  if(needed)
    r_value = gen_assign_cast(c, phi_type.use_type, r_value, else_type);

  LLVMBasicBlockRef else_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMAddIncoming(phi, &l_value, &body_from, 1);

    if(r_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &r_value, &else_from, 1);

    return phi;
  }

  return GEN_NOVALUE;
}

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  gentype_t phi_type;

  if(!gentype(c, type, &phi_type))
    return NULL;

  LLVMBasicBlockRef body_block = codegen_block(c, "repeat_body");
  LLVMBasicBlockRef cond_block = codegen_block(c, "repeat_cond");
  LLVMBasicBlockRef else_block = codegen_block(c, "repeat_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "repeat_post");
  LLVMBuildBr(c->builder, body_block);

  // keep a reference to the cond block
  ast_setdata(ast, cond_block);

  // start the post block so that a break can modify the phi node
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi;

  if(needed)
    phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // body
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef value = gen_expr(c, body);

  if(needed)
    value = gen_assign_cast(c, phi_type.use_type, value, body_type);

  if(value == NULL)
    return NULL;

  // The body evaluates the condition itself, jumping either back to the body
  // or directly to the post block.
  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  LLVMBasicBlockRef body_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildCondBr(c->builder, c_value, post_block, body_block);

  // cond block
  // This is only evaluated from a continue, jumping either back to the body
  // or to the else block.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef i_value = gen_expr(c, cond);
  LLVMBuildCondBr(c->builder, i_value, else_block, body_block);

  // else
  // Only happens for a continue in the last iteration.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef else_value = gen_expr(c, else_clause);

  if(else_value == NULL)
    return NULL;

  if(needed)
    else_value = gen_assign_cast(c, phi_type.use_type, else_value, else_type);

  LLVMBasicBlockRef else_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMAddIncoming(phi, &value, &body_from, 1);

    if(else_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &else_value, &else_from, 1);

    return phi;
  }

  return GEN_NOVALUE;
}

LLVMValueRef gen_break(compile_t* c, ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);
  LLVMBasicBlockRef target = (LLVMBasicBlockRef)ast_data(loop);

  ast_t* body = ast_child(ast);
  ast_t* body_type = ast_type(body);

  LLVMBasicBlockRef break_block = codegen_block(c, "break");
  LLVMBuildBr(c->builder, break_block);

  switch(ast_id(loop))
  {
    case TK_REPEAT:
    {
      // target is cond_block, need to get to the post_block
      target = LLVMGetNextBasicBlock(target); // else
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
  bool needed = (post_phi != NULL) && LLVMIsAPHINode(post_phi);

  // build the break block
  LLVMPositionBuilderAtEnd(c->builder, break_block);
  LLVMValueRef value = gen_expr(c, body);

  if(needed)
  {
    LLVMTypeRef phi_type = LLVMTypeOf(post_phi);
    value = gen_assign_cast(c, phi_type, value, body_type);
  }

  if(value == NULL)
    return NULL;

  break_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, target);

  // add break value to the post block phi node
  if(needed)
    LLVMAddIncoming(post_phi, &value, &break_block, 1);

  return GEN_NOVALUE;
}

LLVMValueRef gen_continue(compile_t* c, ast_t* ast)
{
  // Jump to the loop target.
  ast_t* loop = ast_enclosing_loop(ast);
  LLVMBasicBlockRef target = (LLVMBasicBlockRef)ast_data(loop);
  LLVMBuildBr(c->builder, target);
  return GEN_NOVALUE;
}

LLVMValueRef gen_return(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);

  size_t clause;
  ast_t* try_expr = ast_enclosing_try(ast, &clause);

  // Do the then block only if we return in the body or else clause.
  // In the then block, return without doing the then block.
  if((try_expr != NULL) && (clause != 2))
    gen_expr(c, ast_childidx(try_expr, 2));

  LLVMBuildRet(c->builder, value);
  return GEN_NOVALUE;
}

LLVMValueRef gen_try(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  gentype_t phi_type;

  // we will have no type if both branches have return statements
  if((type != NULL) && !gentype(c, type, &phi_type))
    return NULL;

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef else_block = codegen_block(c, "try_else");
  LLVMBasicBlockRef post_block = NULL;

  if(type != NULL)
    post_block = codegen_block(c, "try_post");

  // keep a reference to the else block
  ast_setdata(ast, else_block);

  // body block
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef body_value = gen_expr(c, body);

  if(body_value != GEN_NOVALUE)
  {
    if(needed)
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
    if(needed)
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

  if(needed)
  {
    LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

    if(body_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &body_value, &block, 1);

    if(else_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &else_value, &else_block, 1);

    return phi;
  }

  return GEN_NOVALUE;
}

LLVMValueRef gen_error(compile_t* c, ast_t* ast)
{
  size_t clause;
  ast_t* try_expr = ast_enclosing_try(ast, &clause);

  // Do the then block only if we error out in the else clause.
  // In the body or the then block, error out without doing the then block.
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
