#ifndef CODEGEN_GENOPERATOR_H
#define CODEGEN_GENOPERATOR_H

#include "codegen.h"

LLVMValueRef gen_lvalue(compile_t* c, ast_t* ast);

LLVMValueRef gen_not(compile_t* c, ast_t* ast);

LLVMValueRef gen_unaryminus(compile_t* c, ast_t* ast);

LLVMValueRef gen_plus(compile_t* c, ast_t* ast);

LLVMValueRef gen_minus(compile_t* c, ast_t* ast);

LLVMValueRef gen_multiply(compile_t* c, ast_t* ast);

LLVMValueRef gen_divide(compile_t* c, ast_t* ast);

LLVMValueRef gen_mod(compile_t* c, ast_t* ast);

LLVMValueRef gen_lshift(compile_t* c, ast_t* ast);

LLVMValueRef gen_rshift(compile_t* c, ast_t* ast);

LLVMValueRef gen_lt(compile_t* c, ast_t* ast);

LLVMValueRef gen_le(compile_t* c, ast_t* ast);

LLVMValueRef gen_ge(compile_t* c, ast_t* ast);

LLVMValueRef gen_gt(compile_t* c, ast_t* ast);

LLVMValueRef gen_eq(compile_t* c, ast_t* ast);

LLVMValueRef gen_ne(compile_t* c, ast_t* ast);

LLVMValueRef gen_is(compile_t* c, ast_t* ast);

LLVMValueRef gen_and(compile_t* c, ast_t* ast);

LLVMValueRef gen_or(compile_t* c, ast_t* ast);

LLVMValueRef gen_xor(compile_t* c, ast_t* ast);

LLVMValueRef gen_assign(compile_t* c, ast_t* ast);

#endif
