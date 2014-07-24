#ifndef CODEGEN_GENCONTROL_H
#define CODEGEN_GENCONTROL_H

#include "codegen.h"

LLVMValueRef gen_expr(compile_t* c, ast_t* ast);

LLVMValueRef gen_lvalue(compile_t* c, ast_t* ast);

LLVMValueRef gen_seq(compile_t* c, ast_t* ast);

LLVMValueRef gen_assign(compile_t* c, ast_t* ast);

LLVMValueRef gen_this(compile_t* c, ast_t* ast);

LLVMValueRef gen_param(compile_t* c, ast_t* ast);

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast);

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast);

LLVMValueRef gen_cast(compile_t* c, ast_t* left, ast_t* right,
  LLVMValueRef l_value, LLVMValueRef r_value);

#endif
