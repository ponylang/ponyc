#include "gencontrol.h"
#include "genexpr.h"
#include "gencall.h"
#include "genname.h"
#include "../pass/expr.h"
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
  bool needed = is_result_needed(ast);
  ast_t* type = ast_type(ast);
  AST_GET_CHILDREN(ast, cond, left, right);

  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  // We will have no type if both branches have return statements.
  reachable_type_t* phi_type = NULL;

  if(!is_control_type(type))
    phi_type = reach_type(c->reachable, type);

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  // If the conditional is constant, generate only one branch.
  bool gen_left = true;
  bool gen_right = true;

  if(LLVMIsAConstantInt(c_value))
  {
    int value = (int)LLVMConstIntGetZExtValue(c_value);

    if(value == 0)
      gen_left = false;
    else
      gen_right = false;
  }

  LLVMBasicBlockRef then_block = codegen_block(c, "if_then");
  LLVMBasicBlockRef else_block = codegen_block(c, "if_else");
  LLVMBasicBlockRef post_block = NULL;

  // If both branches return, we have no post block.
  if(!is_control_type(type))
    post_block = codegen_block(c, "if_post");

  LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  // Left branch.
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef l_value;

  if(gen_left)
  {
    l_value = gen_expr(c, left);
  } else if(phi_type != NULL) {
    l_value = LLVMConstNull(phi_type->use_type);
  } else {
    LLVMBuildUnreachable(c->builder);
    l_value = GEN_NOVALUE;
  }

  if(l_value != GEN_NOVALUE)
  {
    if(needed)
      l_value = gen_assign_cast(c, phi_type->use_type, l_value, left_type);

    if(l_value == NULL)
      return NULL;

    then_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // Right branch.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value;

  if(gen_right)
  {
    r_value = gen_expr(c, right);
  } else if(phi_type != NULL) {
    r_value = LLVMConstNull(phi_type->use_type);
  } else {
    LLVMBuildUnreachable(c->builder);
    r_value = GEN_NOVALUE;
  }

  // If the right side returns, we don't branch to the post block.
  if(r_value != GEN_NOVALUE)
  {
    if(needed)
      r_value = gen_assign_cast(c, phi_type->use_type, r_value, right_type);

    if(r_value == NULL)
      return NULL;

    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // If both sides return, we return a sentinal value.
  if(is_control_type(type))
    return GEN_NOVALUE;

  // Continue in the post block.
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");

    if(l_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &l_value, &then_block, 1);

    if(r_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &r_value, &else_block, 1);

    return phi;
  }

  return GEN_NOTNEEDED;
}

LLVMValueRef gen_while(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  reachable_type_t* phi_type = NULL;

  if(needed && !is_control_type(type))
    phi_type = reach_type(c->reachable, type);

  LLVMBasicBlockRef init_block = codegen_block(c, "while_init");
  LLVMBasicBlockRef body_block = codegen_block(c, "while_body");
  LLVMBasicBlockRef else_block = codegen_block(c, "while_else");
  LLVMBasicBlockRef post_block = NULL;
  LLVMBuildBr(c->builder, init_block);

  // start the post block so that a break can modify the phi node
  LLVMValueRef phi = GEN_NOTNEEDED;

  if(!is_control_type(type))
  {
    // Start the post block so that a break can modify the phi node.
    post_block = codegen_block(c, "match_post");
    LLVMPositionBuilderAtEnd(c->builder, post_block);

    if(needed)
      phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");
  }

  // Push the loop status.
  codegen_pushloop(c, init_block, post_block);

  // init
  // This jumps either to the body or the else clause. This is not evaluated
  // on each loop iteration: only on the first entry or after a continue.
  LLVMPositionBuilderAtEnd(c->builder, init_block);
  LLVMValueRef i_value = gen_expr(c, cond);

  if(i_value == NULL)
    return NULL;

  LLVMBuildCondBr(c->builder, i_value, body_block, else_block);

  // Body.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef l_value = gen_expr(c, body);

  if(needed)
    l_value = gen_assign_cast(c, phi_type->use_type, l_value, body_type);

  if(l_value == NULL)
    return NULL;

  LLVMBasicBlockRef body_from = NULL;

  // If the body can't result in a value, don't generate the conditional
  // evaluation. This basic block for the body already has a terminator.
  if(l_value != GEN_NOVALUE)
  {
    // The body evaluates the condition itself, jumping either back to the body
    // or directly to the post block.
    LLVMValueRef c_value = gen_expr(c, cond);

    if(c_value == NULL)
      return NULL;

    body_from = LLVMGetInsertBlock(c->builder);
    LLVMBuildCondBr(c->builder, c_value, body_block, post_block);
  }

  // Don't need loop status for the else block.
  codegen_poploop(c);

  // else
  // If the loop doesn't generate a value (doesn't execute, or continues on the
  // last iteration), the else clause generates the value.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, else_clause);
  LLVMBasicBlockRef else_from = NULL;

  if(r_value != GEN_NOVALUE)
  {
    if(r_value == NULL)
      return NULL;

    if(needed)
      r_value = gen_assign_cast(c, phi_type->use_type, r_value, else_type);

    else_from = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  if(is_control_type(type))
    return GEN_NOVALUE;

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    if(l_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &l_value, &body_from, 1);

    if(r_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &r_value, &else_from, 1);

    return phi;
  }

  return GEN_NOTNEEDED;
}

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, cond, else_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  reachable_type_t* phi_type = NULL;

  if(needed && !is_control_type(type))
    phi_type = reach_type(c->reachable, type);

  LLVMBasicBlockRef body_block = codegen_block(c, "repeat_body");
  LLVMBasicBlockRef cond_block = codegen_block(c, "repeat_cond");
  LLVMBasicBlockRef else_block = codegen_block(c, "repeat_else");
  LLVMBasicBlockRef post_block = NULL;
  LLVMBuildBr(c->builder, body_block);

  // start the post block so that a break can modify the phi node
  LLVMValueRef phi = GEN_NOTNEEDED;

  if(!is_control_type(type))
  {
    // Start the post block so that a break can modify the phi node.
    post_block = codegen_block(c, "match_post");
    LLVMPositionBuilderAtEnd(c->builder, post_block);

    if(needed)
      phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");
  }

  // Push the loop status.
  codegen_pushloop(c, cond_block, post_block);

  // Body.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef value = gen_expr(c, body);

  if(needed)
    value = gen_assign_cast(c, phi_type->use_type, value, body_type);

  if(value == NULL)
    return NULL;

  LLVMBasicBlockRef body_from = NULL;

  // If the body can't result in a value, don't generate the conditional
  // evaluation. This basic block for the body already has a terminator.
  if(value != GEN_NOVALUE)
  {
    // The body evaluates the condition itself, jumping either back to the body
    // or directly to the post block.
    LLVMValueRef c_value = gen_expr(c, cond);

    if(c_value == NULL)
      return NULL;

    body_from = LLVMGetInsertBlock(c->builder);
    LLVMBuildCondBr(c->builder, c_value, post_block, body_block);
  }

  // cond block
  // This is only evaluated from a continue, jumping either back to the body
  // or to the else block.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef i_value = gen_expr(c, cond);
  LLVMBuildCondBr(c->builder, i_value, else_block, body_block);

  // Don't need loop status for the else block.
  codegen_poploop(c);

  // else
  // Only happens for a continue in the last iteration.
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef else_value = gen_expr(c, else_clause);
  LLVMBasicBlockRef else_from = NULL;

  if(else_value == NULL)
    return NULL;

  if(needed)
    else_value = gen_assign_cast(c, phi_type->use_type, else_value, else_type);

  if(else_value != GEN_NOVALUE)
  {
    else_from = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  if(is_control_type(type))
    return GEN_NOVALUE;

  // post
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    if(value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &value, &body_from, 1);

    if(else_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &else_value, &else_from, 1);

    return phi;
  }

  return GEN_NOTNEEDED;
}

LLVMValueRef gen_break(compile_t* c, ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* body_type = ast_type(body);

  // Get the break target.
  LLVMBasicBlockRef target = c->frame->break_target;

  // Get the phi node.
  LLVMValueRef post_phi = LLVMGetFirstInstruction(target);
  bool needed = (post_phi != NULL) && LLVMIsAPHINode(post_phi);

  // Build the break expression.
  LLVMValueRef value = gen_expr(c, body);

  if(needed)
  {
    // Cast it to the phi type if we need to.
    LLVMTypeRef phi_type = LLVMTypeOf(post_phi);
    value = gen_assign_cast(c, phi_type, value, body_type);
  }

  if(value == NULL)
    return NULL;

  // Add break value to the post block phi node.
  if(needed)
  {
    LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(post_phi, &value, &insert_block, 1);
  }

  // Jump to the break target.
  codegen_debugloc(c, ast);
  LLVMBuildBr(c->builder, target);
  codegen_debugloc(c, NULL);

  return GEN_NOVALUE;
}

LLVMValueRef gen_continue(compile_t* c, ast_t* ast)
{
  (void)ast;

  // Jump to the continue target.
  codegen_debugloc(c, ast);
  LLVMBuildBr(c->builder, c->frame->continue_target);
  codegen_debugloc(c, NULL);

  return GEN_NOVALUE;
}

LLVMValueRef gen_return(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);

  size_t clause;
  ast_t* try_expr = ast_try_clause(ast, &clause);

  // Do the then block only if we return in the body or else clause.
  // In the then block, return without doing the then block.
  if((try_expr != NULL) && (clause != 2))
    gen_expr(c, ast_childidx(try_expr, 2));

  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(codegen_fun(c)));
  LLVMTypeRef r_type = LLVMGetReturnType(f_type);

  codegen_debugloc(c, ast);

  if(LLVMGetTypeKind(r_type) != LLVMVoidTypeKind)
  {
    LLVMValueRef ret = gen_assign_cast(c, r_type, value, ast_type(expr));
    LLVMBuildRet(c->builder, ret);
  } else {
    LLVMBuildRetVoid(c->builder);
  }

  codegen_debugloc(c, NULL);
  return GEN_NOVALUE;
}

LLVMValueRef gen_try(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  ast_t* type = ast_type(ast);
  ast_t* body_type = ast_type(body);
  ast_t* else_type = ast_type(else_clause);

  reachable_type_t* phi_type = NULL;

  // We will have no type if both branches have return statements.
  if(!is_control_type(type))
    phi_type = reach_type(c->reachable, type);

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef else_block = codegen_block(c, "try_else");
  LLVMBasicBlockRef post_block = NULL;

  if(!is_control_type(type))
    post_block = codegen_block(c, "try_post");

  // Keep a reference to the else block.
  codegen_pushtry(c, else_block);

  // Body block.
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef body_value = gen_expr(c, body);

  if(body_value != GEN_NOVALUE)
  {
    if(needed)
    {
      body_value = gen_assign_cast(c, phi_type->use_type, body_value,
        body_type);
    }

    if(body_value == NULL)
      return NULL;

    gen_expr(c, then_clause);
    block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // Pop the try before generating the else block.
  codegen_poptry(c);

  // Else block.
  LLVMPositionBuilderAtEnd(c->builder, else_block);

  // The landing pad is marked as a cleanup, since exceptions are typeless and
  // valueless. The first landing pad is always the destination.
  LLVMTypeRef lp_elements[2];
  lp_elements[0] = c->void_ptr;
  lp_elements[1] = c->i32;
  LLVMTypeRef lp_type = LLVMStructTypeInContext(c->context, lp_elements, 2,
    false);

#if PONY_LLVM == 307 && LLVM_VERSION_PATCH == 0
  // This backwards-incompatible API change to LLVMBuildLandingPad is only in
  // LLVM 3.7.0. In 3.7.1 and all later versions, backward-compatibility was
  // restored.
  assert((c->frame->fun != NULL) && "No function in current frame!");
  LLVMSetPersonalityFn(c->frame->fun, c->personality);
  LLVMValueRef landing = LLVMBuildLandingPad(c->builder, lp_type, 1, "");
#else
  LLVMValueRef landing = LLVMBuildLandingPad(c->builder, lp_type,
    c->personality, 1, "");
#endif

  LLVMAddClause(landing, LLVMConstNull(c->void_ptr));

  LLVMValueRef else_value = gen_expr(c, else_clause);

  if(else_value != GEN_NOVALUE)
  {
    if(needed)
    {
      else_value = gen_assign_cast(c, phi_type->use_type, else_value,
        else_type);
    }

    if(else_value == NULL)
      return NULL;

    gen_expr(c, then_clause);

    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // If both sides return, we return a sentinal value.
  if(is_control_type(type))
    return GEN_NOVALUE;

  // Continue in the post block.
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");

    if(body_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &body_value, &block, 1);

    if(else_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &else_value, &else_block, 1);

    return phi;
  }

  return GEN_NOTNEEDED;
}

LLVMValueRef gen_error(compile_t* c, ast_t* ast)
{
  size_t clause;
  ast_t* try_expr = ast_try_clause(ast, &clause);

  // Do the then block only if we error out in the else clause.
  if((try_expr != NULL) && (clause == 1))
    gen_expr(c, ast_childidx(try_expr, 2));

  codegen_debugloc(c, ast);
  gencall_throw(c);
  codegen_debugloc(c, NULL);

  return GEN_NOVALUE;
}
