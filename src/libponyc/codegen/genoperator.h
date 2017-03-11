#ifndef CODEGEN_GENOPERATOR_H
#define CODEGEN_GENOPERATOR_H

#include<platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_add(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_sub(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_mul(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_div(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_mod(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_neg(compile_t* c, ast_t* ast, bool safe);

LLVMValueRef gen_shl(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_shr(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_and_sc(compile_t* c, ast_t* left, ast_t* right);

LLVMValueRef gen_or_sc(compile_t* c, ast_t* left, ast_t* right);

LLVMValueRef gen_and(compile_t* c, ast_t* left, ast_t* right);

LLVMValueRef gen_or(compile_t* c, ast_t* left, ast_t* right);

LLVMValueRef gen_xor(compile_t* c, ast_t* left, ast_t* right);

LLVMValueRef gen_not(compile_t* c, ast_t* ast);

LLVMValueRef gen_lt(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_le(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_ge(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_gt(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_eq(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_eq_rvalue(compile_t* c, ast_t* left, LLVMValueRef r_value,
  bool safe);

LLVMValueRef gen_ne(compile_t* c, ast_t* left, ast_t* right, bool safe);

LLVMValueRef gen_is(compile_t* c, ast_t* ast);

LLVMValueRef gen_isnt(compile_t* c, ast_t* ast);

LLVMValueRef gen_assign(compile_t* c, ast_t* ast);

LLVMValueRef gen_assign_value(compile_t* c, ast_t* left, LLVMValueRef right,
  ast_t* right_type);

PONY_EXTERN_C_END

#endif
