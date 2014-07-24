#ifndef CODEGEN_GENOPERATOR_H
#define CODEGEN_GENOPERATOR_H

#include "codegen.h"

LLVMValueRef gen_lvalue(compile_t* c, ast_t* ast);

LLVMValueRef gen_unaryminus(compile_t* c, ast_t* ast);

LLVMValueRef gen_plus(compile_t* c, ast_t* ast);

LLVMValueRef gen_minus(compile_t* c, ast_t* ast);

LLVMValueRef gen_multiply(compile_t* c, ast_t* ast);

LLVMValueRef gen_divide(compile_t* c, ast_t* ast);

LLVMValueRef gen_assign(compile_t* c, ast_t* ast);

#endif
