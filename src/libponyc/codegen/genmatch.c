#include "genmatch.h"
#include "gentype.h"
#include "gendesc.h"
#include "genexpr.h"
#include "../type/subtype.h"
#include <assert.h>

static LLVMValueRef runtime_type(compile_t* c, LLVMValueRef value, ast_t* type);

static LLVMValueRef make_binop(compile_t* c, LLVMValueRef l_value,
  LLVMValueRef r_value, LLVMOpcode op)
{
  // TODO: short circuit manually?
  if(l_value == NULL)
    return r_value;

  if(r_value == NULL)
    return l_value;

  return LLVMBuildBinOp(c->builder, op, l_value, r_value, "");
}

static LLVMValueRef loop_binop(compile_t* c, LLVMValueRef value, ast_t* type,
  LLVMOpcode op)
{
  ast_t* child_type = ast_child(type);
  LLVMValueRef result = NULL;

  while(child_type != NULL)
  {
    LLVMValueRef child_match = runtime_type(c, value, child_type);
    result = make_binop(c, result, child_match, op);
    child_type = ast_sibling(child_type);
  }

  return result;
}

static LLVMValueRef runtime_type(compile_t* c, LLVMValueRef value, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return loop_binop(c, value, type, LLVMOr);

    case TK_ISECTTYPE:
      return loop_binop(c, value, type, LLVMAnd);

    case TK_TUPLETYPE:
    {
      // TODO: pairwise check
      break;
    }

    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);

      if(ast_id(def) == TK_TRAIT)
        return gendesc_istrait(c, value, type);

      return gendesc_isentity(c, value, type);
    }

    case TK_STRUCTURAL:
    {
      // Shouldn't get here. Should only accept structural types in patterns
      // that are supertypes of the match type.
      return LLVMConstInt(c->i1, 0, false);
    }

    case TK_ARROW:
      return runtime_type(c, value, ast_childidx(type, 1));

    default: {}
  }

  assert(0);
  return NULL;
}

static bool case_body(compile_t* c, LLVMBasicBlockRef body_block, ast_t* body,
  LLVMBasicBlockRef post_block, LLVMValueRef phi, LLVMTypeRef phi_type)
{
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef body_value = gen_expr(c, body);

  // If it returns, we don't branch to the post block.
  if(body_value == GEN_NOVALUE)
    return true;

  ast_t* body_type = ast_type(body);
  body_value = gen_assign_cast(c, phi_type, body_value, body_type);

  if(body_value == NULL)
    return false;

  body_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);
  LLVMAddIncoming(phi, &body_value, &body_block, 1);

  return true;
}

LLVMValueRef gen_match(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, match_expr, cases, else_expr);

  ast_t* type = ast_type(ast);
  gentype_t phi_type;

  // We will have no type if all branches have return statements.
  if((type != NULL) && !gentype(c, type, &phi_type))
    return NULL;

  ast_t* match_type = ast_type(match_expr);
  LLVMValueRef match_value = gen_expr(c, match_expr);

  LLVMBasicBlockRef case_block = codegen_block(c, "case");
  LLVMBasicBlockRef else_block = codegen_block(c, "match_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "match_post");
  LLVMBasicBlockRef next_block;

  // Jump to the first case.
  LLVMBuildBr(c->builder, case_block);

  // Start the post block so that a case can modify the phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // Iterate over the cases.
  ast_t* the_case = ast_child(cases);

  while(the_case != NULL)
  {
    LLVMPositionBuilderAtEnd(c->builder, case_block);
    LLVMValueRef matched = NULL;

    ast_t* next_case = ast_sibling(the_case);

    if(next_case != NULL)
      next_block = codegen_block(c, "case");
    else
      next_block = else_block;

    AST_GET_CHILDREN(the_case, pattern, guard, body);
    ast_t* pattern_type = ast_type(pattern);

    // If the match expression isn't a subtype of the pattern expression,
    // check at runtime if the type matches.
    if(!is_subtype(match_type, pattern_type))
      matched = runtime_type(c, match_value, pattern_type);

    // TODO: check values as well as types in pattern and assign to variables
    // in the patterns
    // TODO: check the guard

    LLVMBasicBlockRef body_block = codegen_block(c, "case_body");
    LLVMBuildCondBr(c->builder, matched, body_block, next_block);

    // Case body.
    if(!case_body(c, body_block, body, post_block, phi, phi_type.use_type))
      return NULL;

    the_case = next_case;
    case_block = next_block;
  }

  // Else body.
  if(!case_body(c, else_block, else_expr, post_block, phi, phi_type.use_type))
    return NULL;

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  return phi;
}
