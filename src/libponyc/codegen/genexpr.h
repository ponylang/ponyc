#ifndef CODEGEN_GENEXPR_H
#define CODEGEN_GENEXPR_H

#include "codegen.h"

LLVMValueRef gen_expr(compile_t* c, ast_t* ast);

/**
 * Generates the left and right expressions, handles binop casting.
 *
 * Returns true if both sides are constants, false otherwise.
 */
bool gen_binop(compile_t* c, ast_t* ast,
  LLVMValueRef* l_value, LLVMValueRef* r_value);

/**
 * Returns lit cast to the type of val.
 */
LLVMValueRef gen_literal_cast(LLVMValueRef lit, LLVMValueRef val, bool sign);

/**
 * If one or both sides are literals, the values are cast to the correct types.
 *
 * Returns true if both sides are constants, false otherwise
 */
bool gen_binop_cast(ast_t* left, ast_t* right, LLVMValueRef* pl_value,
  LLVMValueRef* pr_value);

/**
 * Returns r_value cast to l_type.
 *
 * This will box primitives when necessary.
 */
LLVMValueRef gen_assign_cast(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, bool sign);

#endif
