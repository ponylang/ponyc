#include "genident.h"
#include "gentype.h"
#include "gendesc.h"
#include "genexpr.h"
#include <assert.h>

static LLVMValueRef tuple_is(compile_t* c, ast_t* left_type, ast_t* right_type,
  LLVMValueRef l_value, LLVMValueRef r_value)
{
  assert(ast_id(left_type) == TK_TUPLETYPE);
  assert(ast_id(right_type) == TK_TUPLETYPE);
  assert(ast_childcount(left_type) == ast_childcount(right_type));

  // Pairwise comparison.
  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Set up the phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  ast_t* left_child = ast_child(left_type);
  ast_t* right_child = ast_child(right_type);
  int i = 0;

  while(left_child != NULL)
  {
    // Set up the next block.
    LLVMBasicBlockRef next_block = codegen_block(c, "next");
    LLVMPositionBuilderAtEnd(c->builder, this_block);

    // Test the element.
    LLVMValueRef l_elem = LLVMBuildExtractValue(c->builder, l_value, i, "");
    LLVMValueRef r_elem = LLVMBuildExtractValue(c->builder, r_value, i, "");
    LLVMValueRef test = gen_is_value(c, left_child, right_child, l_elem,
      r_elem);

    // If false, go directly to the post block.
    LLVMBuildCondBr(c->builder, test, next_block, post_block);
    LLVMAddIncoming(phi, &test, &this_block, 1);

    // Point to the next block.
    this_block = next_block;

    left_child = ast_sibling(left_child);
    right_child = ast_sibling(right_child);
    i++;
  }

  // The last block is reached if every element returns true. Jump directly to
  // the post block.
  LLVMPositionBuilderAtEnd(c->builder, this_block);
  LLVMBuildBr(c->builder, post_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef one = LLVMConstInt(c->i1, 1, false);
  LLVMAddIncoming(phi, &one, &this_block, 1);

  return phi;
}

LLVMValueRef gen_is_value(compile_t* c, ast_t* left_type, ast_t* right_type,
  LLVMValueRef l_value, LLVMValueRef r_value)
{
  LLVMTypeRef l_type = LLVMTypeOf(l_value);
  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
    {
      // If it's the same type, compare.
      if(l_type == r_type)
        return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");

      // It can't have the same identity, even if it's the same value.
      return LLVMConstInt(c->i1, 0, false);
    }

    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    {
      // If it's the same type, just compare.
      if(l_type == r_type)
        return LLVMBuildFCmp(c->builder, LLVMRealOEQ, l_value, r_value, "");

      // It can't have the same identity, even if it's the same value.
      return LLVMConstInt(c->i1, 0, false);
    }

    case LLVMStructTypeKind:
    {
      // Pairwise comparison.
      if(LLVMGetTypeKind(r_type) == LLVMStructTypeKind)
        return tuple_is(c, left_type, right_type, l_value, r_value);

      // It can't have the same identity, even if it's the same value.
      return LLVMConstInt(c->i1, 0, false);
    }

    case LLVMPointerTypeKind:
    {
      // It can't have the same identity, even if it's the same value.
      if(LLVMGetTypeKind(r_type) != LLVMPointerTypeKind)
        return LLVMConstInt(c->i1, 0, false);

      // Pointers must be to the same address.
      l_value = LLVMBuildPtrToInt(c->builder, l_value, c->intptr, "");
      r_value = LLVMBuildPtrToInt(c->builder, r_value, c->intptr, "");
      return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");
    }

    default: {}
  }

  assert(0);
  return NULL;
}

LLVMValueRef gen_is(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);
  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  return gen_is_value(c, left_type, right_type, l_value, r_value);
}

LLVMValueRef gen_isnt(compile_t* c, ast_t* ast)
{
  LLVMValueRef is = gen_is(c, ast);

  if(is == NULL)
    return NULL;

  return LLVMBuildNot(c->builder, is, "");
}
