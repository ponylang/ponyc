#include "gencontrol.h"
#include "gencall.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "../pass/expr.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

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
  AST_GET_CHILDREN(ast, cond, left, right);

  LLVMValueRef c_value = gen_expr(c, cond);

  if(c_value == NULL)
    return NULL;

  deferred_reification_t* reify = c->frame->reify;

  // We will have no type if both branches have return statements.
  compile_type_t* phi_type = NULL;

  if(needed && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
  {
    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    phi_type = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);
  }

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
  if(!ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    post_block = codegen_block(c, "if_post");

  LLVMValueRef br = LLVMBuildCondBr(c->builder, c_value, then_block, else_block);

  handle_branch_prediction_default(c->context, br, ast);

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
    {
      ast_t* left_type = ast_type(left);

      // May be NULL if we're in an ifdef with a compile_error.
      if(left_type != NULL)
      {
        left_type = deferred_reify(reify, left_type, c->opt);
        l_value = gen_assign_cast(c, phi_type->use_type, l_value, left_type);
        ast_free_unattached(left_type);
      }
    }

    if(l_value == NULL)
      return NULL;

    then_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // Right branch.
  LLVMMoveBasicBlockAfter(else_block, LLVMGetInsertBlock(c->builder));
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
    {
      ast_t* right_type = ast_type(right);

      // May be NULL if we're in an ifdef with a compile_error.
      if(right_type != NULL)
      {
        right_type = deferred_reify(reify, right_type, c->opt);
        r_value = gen_assign_cast(c, phi_type->use_type, r_value, right_type);
        ast_free_unattached(right_type);
      }
    }

    if(r_value == NULL)
      return NULL;

    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // If both sides return, we return a sentinel value.
  if(ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    return GEN_NOVALUE;

  // Continue in the post block.
  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
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

LLVMValueRef gen_iftype(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);
  AST_GET_CHILDREN(left, subtype, supertype, body);

  deferred_reification_t* reify = c->frame->reify;

  ast_t* r_sub = deferred_reify(reify, subtype, c->opt);
  ast_t* r_super = deferred_reify(reify, supertype, c->opt);

  bool is_sub = is_subtype_constraint(r_sub, r_super, NULL, c->opt);

  ast_free_unattached(r_sub);
  ast_free_unattached(r_super);

  if(is_sub)
    return gen_expr(c, body);

  return gen_expr(c, right);
}

LLVMValueRef gen_while(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, cond, body, else_clause);

  deferred_reification_t* reify = c->frame->reify;

  compile_type_t* phi_type = NULL;

  if(needed && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
  {
    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    phi_type = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);
  }

  LLVMBasicBlockRef init_block = codegen_block(c, "while_init");
  LLVMBasicBlockRef body_block = codegen_block(c, "while_body");
  LLVMBasicBlockRef else_block = codegen_block(c, "while_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "while_post");
  LLVMBuildBr(c->builder, init_block);

  // Start a phi node in the post block if needed.
  LLVMValueRef phi = GEN_NOTNEEDED;
  if(!ast_checkflag(ast, AST_FLAG_JUMPS_AWAY) && needed)
  {
    LLVMPositionBuilderAtEnd(c->builder, post_block);
    phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");
  }

  // Push the loop status.
  codegen_pushloop(c, init_block, post_block, else_block);

  // init
  // This jumps either to the body or the else clause. This is not evaluated
  // on each loop iteration: only on the first entry or after a continue.
  LLVMPositionBuilderAtEnd(c->builder, init_block);
  LLVMValueRef i_value = gen_expr(c, cond);

  if(i_value == NULL)
    return NULL;

  LLVMValueRef br = LLVMBuildCondBr(c->builder, i_value, body_block, else_block);

  handle_branch_prediction_default(c->context, br, ast);

  // Body.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef l_value = gen_expr(c, body);

  if(needed && (ast_type(body) != NULL))
  {
    ast_t* body_type = deferred_reify(reify, ast_type(body), c->opt);
    l_value = gen_assign_cast(c, phi_type->use_type, l_value, body_type);
    ast_free_unattached(body_type);
  }

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
    br = LLVMBuildCondBr(c->builder, c_value, body_block, post_block);

    handle_branch_prediction_default(c->context, br, ast);
  }

  // Don't need loop status for the else block.
  codegen_poploop(c);

  // else
  // If the loop doesn't generate a value (doesn't execute, or continues on the
  // last iteration), the else clause generates the value.
  LLVMMoveBasicBlockAfter(else_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef r_value = gen_expr(c, else_clause);
  LLVMBasicBlockRef else_from = NULL;

  if(r_value != GEN_NOVALUE)
  {
    if(r_value == NULL)
      return NULL;

    if(needed)
    {
      ast_t* else_type = deferred_reify(reify, ast_type(else_clause), c->opt);
      r_value = gen_assign_cast(c, phi_type->use_type, r_value, else_type);
      ast_free_unattached(else_type);
    }

    else_from = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // post
  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    return GEN_NOVALUE;

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

  deferred_reification_t* reify = c->frame->reify;

  compile_type_t* phi_type = NULL;

  if(needed && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
  {
    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    phi_type = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);
  }

  LLVMBasicBlockRef body_block = codegen_block(c, "repeat_body");
  LLVMBasicBlockRef cond_block = codegen_block(c, "repeat_cond");
  LLVMBasicBlockRef else_block = codegen_block(c, "repeat_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "repeat_post");
  LLVMBuildBr(c->builder, body_block);

  // Start a phi node in the post block if needed.
  LLVMValueRef phi = GEN_NOTNEEDED;
  if(!ast_checkflag(ast, AST_FLAG_JUMPS_AWAY) && needed)
  {
    LLVMPositionBuilderAtEnd(c->builder, post_block);
    phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");
  }

  // Push the loop status.
  codegen_pushloop(c, cond_block, post_block, else_block);

  // Body.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef value = gen_expr(c, body);

  if(needed && (ast_type(body) != NULL))
  {
    ast_t* body_type = deferred_reify(reify, ast_type(body), c->opt);
    value = gen_assign_cast(c, phi_type->use_type, value, body_type);
    ast_free_unattached(body_type);
  }

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
    LLVMValueRef br = LLVMBuildCondBr(c->builder, c_value, post_block,
      body_block);

    handle_branch_prediction_default(c->context, br, cond);
  }

  // cond block
  // This is only evaluated from a continue, jumping either back to the body
  // or to the else block.
  LLVMMoveBasicBlockAfter(cond_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef i_value = gen_expr(c, cond);

  LLVMValueRef br = LLVMBuildCondBr(c->builder, i_value, else_block, body_block);

  handle_branch_prediction_default(c->context, br, cond);

  // Don't need loop status for the else block.
  codegen_poploop(c);

  // else
  // Only happens for a continue in the last iteration.
  LLVMMoveBasicBlockAfter(else_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, else_block);
  LLVMValueRef else_value = gen_expr(c, else_clause);
  LLVMBasicBlockRef else_from = NULL;

  if(else_value == NULL)
    return NULL;

  if(needed)
  {
    ast_t* else_type = deferred_reify(reify, ast_type(else_clause), c->opt);
    else_value = gen_assign_cast(c, phi_type->use_type, else_value, else_type);
    ast_free_unattached(else_type);
  }

  if(else_value != GEN_NOVALUE)
  {
    else_from = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // post
  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    return GEN_NOVALUE;

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

LLVMValueRef gen_recover(compile_t* c, ast_t* ast)
{
  ast_t* body = ast_childidx(ast, 1);
  LLVMValueRef ret = gen_expr(c, body);

  if(is_result_needed(ast))
  {
    deferred_reification_t* reify = c->frame->reify;

    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    compile_type_t* c_t = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);

    type = deferred_reify(reify, ast_type(body), c->opt);
    ret = gen_assign_cast(c, c_t->use_type, ret, type);
    ast_free_unattached(type);
  }

  return ret;
}

static void gen_then_clauses(compile_t* c, ast_t* ast, bool within_loop)
{
  pony_assert(ast != NULL);
  // search upward the current ast and generate all try-then clauses up to
  // the first loop to which we are continuing here if within_loop is true
  // else up to the enclosing fun, be or constructor
  ast_t* last = ast;
  ast_t* parent = ast_parent(ast);

  while (parent != NULL)
  {
    switch(ast_id(parent))
    {
      case TK_FOR:
      case TK_REPEAT:
      case TK_WHILE:
      {
        if (within_loop)
          return;
        break;
      }

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
        return;

      case TK_TRY:
      case TK_TRY_NO_CHECK:
      {
        if(ast_index(last) != 2)
          gen_expr(c, ast_childidx(parent, 2));
        break;
      }
      case TK_DISPOSING_BLOCK:
      {
        if(ast_index(last) != 1)
          gen_expr(c, ast_childidx(parent, 1));
        break;
      }

      default: {}
    }
    last = parent;
    parent = ast_parent(parent);
  }
}

LLVMValueRef gen_break(compile_t* c, ast_t* ast)
{
  ast_t* body = ast_child(ast);

  LLVMBasicBlockRef target;

  gen_then_clauses(c, ast, true);

  if(ast_id(body) == TK_NONE)
  {
    target = c->frame->break_novalue_target;
  } else {
    // Get the break target.
    target = c->frame->break_target;

    // Get the phi node.
    LLVMValueRef post_phi = LLVMGetFirstInstruction(target);
    bool needed = (post_phi != NULL) && LLVMIsAPHINode(post_phi);

    // Build the break expression.
    LLVMValueRef value = gen_expr(c, body);

    if(needed)
    {
      // Cast it to the phi type if we need to.
      LLVMTypeRef phi_type = LLVMTypeOf(post_phi);
      ast_t* body_type = deferred_reify(c->frame->reify, ast_type(body),
        c->opt);
      value = gen_assign_cast(c, phi_type, value, body_type);
      ast_free_unattached(body_type);
    }

    if(value == NULL)
      return NULL;

    // Add break value to the post block phi node.
    if(needed)
    {
      LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(c->builder);
      LLVMAddIncoming(post_phi, &value, &insert_block, 1);
    }
  }

  // Jump to the break target.
  codegen_scope_lifetime_end(c);
  codegen_debugloc(c, ast);
  LLVMBuildBr(c->builder, target);
  codegen_debugloc(c, NULL);

  return GEN_NOVALUE;
}

LLVMValueRef gen_continue(compile_t* c, ast_t* ast)
{
  gen_then_clauses(c, ast, true);

  // Jump to the continue target.
  codegen_scope_lifetime_end(c);
  codegen_debugloc(c, ast);
  LLVMBuildBr(c->builder, c->frame->continue_target);
  codegen_debugloc(c, NULL);

  return GEN_NOVALUE;
}

LLVMValueRef gen_return(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);

  gen_then_clauses(c, ast, false);

  LLVMTypeRef f_type = LLVMGlobalGetValueType(codegen_fun(c));
  LLVMTypeRef r_type = LLVMGetReturnType(f_type);

  codegen_debugloc(c, ast);

  if(LLVMGetTypeKind(r_type) != LLVMVoidTypeKind)
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(expr), c->opt);
    LLVMValueRef ret = gen_assign_cast(c, r_type, value, type);
    ast_free_unattached(type);
    codegen_scope_lifetime_end(c);
    genfun_build_ret(c, ret);
  } else {
    codegen_scope_lifetime_end(c);
    genfun_build_ret_void(c);
  }

  codegen_debugloc(c, NULL);
  return GEN_NOVALUE;
}

LLVMValueRef gen_try(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, else_clause, then_clause);

  deferred_reification_t* reify = c->frame->reify;

  compile_type_t* phi_type = NULL;

  // We will have no type if both branches have return statements.
  if(needed && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
  {
    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    phi_type = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);
  }

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef else_block = codegen_block(c, "try_else");
  LLVMBasicBlockRef post_block = NULL;

  if(!ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
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
      ast_t* body_type = deferred_reify(reify, ast_type(body), c->opt);
      body_value = gen_assign_cast(c, phi_type->use_type, body_value,
        body_type);
      ast_free_unattached(body_type);
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
  LLVMMoveBasicBlockAfter(else_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, else_block);

  // The landing pad is marked as a cleanup, since exceptions are typeless and
  // valueless. The first landing pad is always the destination.
  LLVMTypeRef lp_elements[2];
  lp_elements[0] = c->ptr;
  lp_elements[1] = c->i32;
  LLVMTypeRef lp_type = LLVMStructTypeInContext(c->context, lp_elements, 2,
    false);

  LLVMValueRef landing = LLVMBuildLandingPad(c->builder, lp_type,
    c->personality, 1, "");

  LLVMAddClause(landing, LLVMConstNull(c->ptr));

  LLVMValueRef else_value = gen_expr(c, else_clause);

  if(else_value != GEN_NOVALUE)
  {
    if(needed)
    {
      ast_t* else_type = deferred_reify(reify, ast_type(else_clause), c->opt);
      else_value = gen_assign_cast(c, phi_type->use_type, else_value,
        else_type);
      ast_free_unattached(else_type);
    }

    if(else_value == NULL)
      return NULL;

    gen_expr(c, then_clause);

    else_block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // If both sides return, we return a sentinel value.
  if(ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    return GEN_NOVALUE;

  // Continue in the post block.
  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
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

LLVMValueRef gen_disposing_block_can_error(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, dispose_clause);

  deferred_reification_t* reify = c->frame->reify;

  compile_type_t* phi_type = NULL;

  // We will have no type if our body has a return statement
  if(needed && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
  {
    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    phi_type = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);
  }

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef else_block = NULL;
  LLVMBasicBlockRef post_block = NULL;

  else_block = codegen_block(c, "disposing_block_else");

  if(!ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    post_block = codegen_block(c, "disposing_block_post");

  // Keep a reference to the else block.
  codegen_pushtry(c, else_block);

  // Body block.
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef body_value = gen_expr(c, body);

  if(body_value != GEN_NOVALUE)
  {
    if(needed)
    {
      ast_t* body_type = deferred_reify(reify, ast_type(body), c->opt);
      body_value = gen_assign_cast(c, phi_type->use_type, body_value,
        body_type);
      ast_free_unattached(body_type);
    }

    if(body_value == NULL)
      return NULL;

    gen_expr(c, dispose_clause);
    block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // Pop the try before generating the else block.
  codegen_poptry(c);

  // we need to create an else that rethrows the error
  // Else block.
  LLVMMoveBasicBlockAfter(else_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, else_block);

  // The landing pad is marked as a cleanup, since exceptions are typeless and
  // valueless. The first landing pad is always the destination.
  LLVMTypeRef lp_elements[2];
  lp_elements[0] = c->ptr;
  lp_elements[1] = c->i32;
  LLVMTypeRef lp_type = LLVMStructTypeInContext(c->context, lp_elements, 2,
    false);

  LLVMValueRef landing = LLVMBuildLandingPad(c->builder, lp_type,
    c->personality, 1, "");

  LLVMAddClause(landing, LLVMConstNull(c->ptr));

  gen_expr(c, dispose_clause);
  gen_error(c, ast_parent(ast));

  // If we jump away, we return a sentinel value.
  if(ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    return GEN_NOVALUE;

  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");

    if(body_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &body_value, &block, 1);

    return phi;
  }

  return GEN_NOTNEEDED;
}

LLVMValueRef gen_disposing_block_cant_error(compile_t* c, ast_t* ast)
{
  bool needed = is_result_needed(ast);
  AST_GET_CHILDREN(ast, body, dispose_clause);

  deferred_reification_t* reify = c->frame->reify;

  compile_type_t* phi_type = NULL;

  // We will have no type if our body has a return statement
  if(needed && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
  {
    ast_t* type = deferred_reify(reify, ast_type(ast), c->opt);
    phi_type = (compile_type_t*)reach_type(c->reach, type)->c_type;
    ast_free_unattached(type);
  }

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef post_block = NULL;

  if(!ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    post_block = codegen_block(c, "disposing_block_post");

  // Body block.
  LLVMPositionBuilderAtEnd(c->builder, block);
  LLVMValueRef body_value = gen_expr(c, body);

  if(body_value != GEN_NOVALUE)
  {
    if(needed)
    {
      ast_t* body_type = deferred_reify(reify, ast_type(body), c->opt);
      body_value = gen_assign_cast(c, phi_type->use_type, body_value,
        body_type);
      ast_free_unattached(body_type);
    }

    if(body_value == NULL)
      return NULL;

    gen_expr(c, dispose_clause);
    block = LLVMGetInsertBlock(c->builder);
    LLVMBuildBr(c->builder, post_block);
  }

  // If we jump away, we return a sentinel value.
  if(ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
    return GEN_NOVALUE;

  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);

  if(needed)
  {
    LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type->use_type, "");

    if(body_value != GEN_NOVALUE)
      LLVMAddIncoming(phi, &body_value, &block, 1);

    return phi;
  }

  return GEN_NOTNEEDED;
}

LLVMValueRef gen_disposing_block(compile_t* c, ast_t* ast)
{
  if(ast_canerror(ast))
    return gen_disposing_block_can_error(c, ast);
  else
    return gen_disposing_block_cant_error(c, ast);
}

LLVMValueRef gen_error(compile_t* c, ast_t* ast)
{
  size_t clause;
  ast_t* error_handler_expr = ast_error_handling_clause(ast, &clause);

  if (error_handler_expr != NULL)
  {
    switch(ast_id(error_handler_expr))
    {
      case TK_TRY:
      case TK_TRY_NO_CHECK:
      {
        // Do the then block only if we error out in the else clause.
        if((error_handler_expr != NULL) && (clause == 1))
          gen_expr(c, ast_childidx(error_handler_expr, 2));
      }
      break;

      case TK_DISPOSING_BLOCK:
      {
        if((error_handler_expr != NULL) && (clause == 0))
          gen_expr(c, ast_childidx(error_handler_expr, 1));
      }
      break;

      default: {}
    }
  }

  codegen_scope_lifetime_end(c);
  codegen_debugloc(c, ast);
  gencall_error(c);
  codegen_debugloc(c, NULL);

  return GEN_NOVALUE;
}

void attach_branchweights_metadata(LLVMContextRef ctx, LLVMValueRef branch,
   unsigned int weights[], unsigned int count)
{
  size_t alloc_index = ponyint_pool_index((count + 1) * sizeof(LLVMValueRef));

  LLVMValueRef* params = (LLVMValueRef*)ponyint_pool_alloc(alloc_index);

  const char str[] = "branch_weights";
  params[0] = LLVMMDStringInContext(ctx, str, sizeof(str) - 1);

  for(size_t i = 0; i < count; i++)
    params[i+1] = LLVMConstInt(LLVMInt32TypeInContext(ctx), weights[i], false);

  LLVMValueRef metadata = LLVMMDNodeInContext(ctx, params, count + 1);
  const char id[] = "prof";
  LLVMSetMetadata(branch, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);

  ponyint_pool_free(alloc_index, params);
}

void handle_branch_prediction_default(LLVMContextRef ctx, LLVMValueRef branch,
  ast_t* ast)
{
  if(ast_has_annotation(ast, "likely"))
  {
    unsigned int weights[] =
      {PONY_BRANCHWEIGHT_LIKELY, PONY_BRANCHWEIGHT_UNLIKELY};
    attach_branchweights_metadata(ctx, branch, weights, 2);
  } else if(ast_has_annotation(ast, "unlikely")) {
    unsigned int weights[] =
      {PONY_BRANCHWEIGHT_UNLIKELY, PONY_BRANCHWEIGHT_LIKELY};
    attach_branchweights_metadata(ctx, branch, weights, 2);
  }
}
