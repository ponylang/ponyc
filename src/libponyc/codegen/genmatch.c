#include "genmatch.h"
#include "gentype.h"
#include "gendesc.h"
#include "genexpr.h"
#include "../type/subtype.h"
#include <assert.h>

#if 0
static LLVMValueRef match_with_type(compile_t* c, LLVMValueRef value,
  ast_t* match_type, ast_t* pattern);

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

static LLVMValueRef loop_binop(compile_t* c, LLVMValueRef value,
  ast_t* match_type, ast_t* pattern_type, LLVMOpcode op)
{
  ast_t* child_type = ast_child(pattern_type);
  LLVMValueRef result = NULL;

  while(child_type != NULL)
  {
    LLVMValueRef child_match = match_with_type(c, value, match_type,
      child_type);
    result = make_binop(c, result, child_match, op);
    child_type = ast_sibling(child_type);
  }

  return result;
}

static LLVMValueRef match_with_type(compile_t* c, LLVMValueRef value,
  ast_t* match_type, ast_t* pattern)
{
  ast_t* pattern_type = ast_type(pattern);

  if(is_math_compatible(match_type, pattern_type) ||
    is_subtype(match_type, pattern_type)
    )
    return LLVMConstInt(c->i1, 1, false);

  switch(ast_id(pattern_type))
  {
    case TK_UNIONTYPE:
      return loop_binop(c, value, match_type, pattern_type, LLVMOr);

    case TK_ISECTTYPE:
      return loop_binop(c, value, match_type, pattern_type, LLVMAnd);

    case TK_TUPLETYPE:
    {
      if(ast_id(match_type) == TK_TUPLETYPE)
      {
        ast_t* match_child = ast_child(match_type);
        ast_t* pattern_child = ast_child(pattern_type);
        LLVMValueRef matched = NULL;
        int i = 0;

        while(match_child != NULL)
        {
          // Check each element of the tuple.
          LLVMValueRef field = LLVMBuildExtractValue(c->builder, value, i, "");
          LLVMValueRef this_match = runtime_type(c, field, match_child,
            pattern_child);
          matched = make_binop(c, matched, this_match, LLVMAnd);

          match_child = ast_sibling(match_child);
          pattern_child = ast_sibling(pattern_child);
          i++;
        }

        assert(pattern_child == NULL);
        return matched;
      }

      // TODO: value is a c->object_ptr
      // either a union with this tuple in it or Any
      break;
    }

    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(pattern_type);

      switch(ast_id(def))
      {
        case TK_TRAIT:
          return gendesc_istrait(c, value, pattern_type);

        case TK_PRIMITIVE:
          return gendesc_isprimitive(c, value, pattern_type);

        case TK_CLASS:
        case TK_ACTOR:
          return gendesc_isentity(c, value, pattern_type);

        default: {}
      }
      break;
    }

    case TK_STRUCTURAL:
    {
      // Shouldn't get here. Should only accept structural types in patterns
      // that are supertypes of the match type.
      return LLVMConstInt(c->i1, 0, false);
    }

    case TK_ARROW:
      return runtime_type(c, value, match_type, ast_childidx(pattern_type, 1));

    default: {}
  }

  assert(0);
  return NULL;
}
#endif

static bool pattern_match(compile_t* c, LLVMValueRef match_value,
  ast_t* match_type, ast_t* pattern, LLVMBasicBlockRef guard_block,
  LLVMBasicBlockRef next_block)
{
  if(ast_id(pattern) == TK_NONE)
  {
    LLVMBuildBr(c->builder, guard_block);
    return true;
  }

  // TODO:
  // LLVMValueRef test = match_with_type(c, match_value, match_type, pattern);
  LLVMValueRef test = LLVMConstInt(c->i1, 1, false);

  if(test == NULL)
    return false;

  LLVMBuildCondBr(c->builder, test, guard_block, next_block);
  return true;
}

static bool guard_match(compile_t* c, ast_t* guard,
  LLVMBasicBlockRef body_block, LLVMBasicBlockRef next_block)
{
  if(ast_id(guard) == TK_NONE)
  {
    LLVMBuildBr(c->builder, body_block);
    return true;
  }

  LLVMValueRef test = gen_expr(c, guard);

  if(test == NULL)
    return false;

  LLVMBuildCondBr(c->builder, test, body_block, next_block);
  return true;
}

static bool case_body(compile_t* c, ast_t* body,
  LLVMBasicBlockRef post_block, LLVMValueRef phi, LLVMTypeRef phi_type)
{
  LLVMValueRef body_value = gen_expr(c, body);

  // If it returns, we don't branch to the post block.
  if(body_value == GEN_NOVALUE)
    return true;

  ast_t* body_type = ast_type(body);
  body_value = gen_assign_cast(c, phi_type, body_value, body_type);

  if(body_value == NULL)
    return false;

  LLVMBuildBr(c->builder, post_block);

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &body_value, &block, 1);

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

  LLVMBasicBlockRef pattern_block = codegen_block(c, "case_pattern");
  LLVMBasicBlockRef else_block = codegen_block(c, "match_else");
  LLVMBasicBlockRef post_block = codegen_block(c, "match_post");
  LLVMBasicBlockRef next_block;

  // Jump to the first case.
  LLVMBuildBr(c->builder, pattern_block);

  // Start the post block so that a case can modify the phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, phi_type.use_type, "");

  // Iterate over the cases.
  ast_t* the_case = ast_child(cases);

  while(the_case != NULL)
  {
    ast_t* next_case = ast_sibling(the_case);

    if(next_case != NULL)
      next_block = codegen_block(c, "case_pattern");
    else
      next_block = else_block;

    AST_GET_CHILDREN(the_case, pattern, guard, body);

    LLVMBasicBlockRef guard_block = codegen_block(c, "case_guard");
    LLVMBasicBlockRef body_block = codegen_block(c, "case_body");

    // Check the pattern.
    LLVMPositionBuilderAtEnd(c->builder, pattern_block);

    if(!pattern_match(c, match_value, match_type, pattern, guard_block,
      next_block))
      return NULL;

    // Check the guard.
    LLVMPositionBuilderAtEnd(c->builder, guard_block);

    if(!guard_match(c, guard, body_block, next_block))
      return NULL;

    // Case body.
    LLVMPositionBuilderAtEnd(c->builder, body_block);

    if(!case_body(c, body, post_block, phi, phi_type.use_type))
      return NULL;

    the_case = next_case;
    pattern_block = next_block;
  }

  // Else body.
  LLVMPositionBuilderAtEnd(c->builder, else_block);

  if(!case_body(c, else_expr, post_block, phi, phi_type.use_type))
    return NULL;

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  return phi;
}
