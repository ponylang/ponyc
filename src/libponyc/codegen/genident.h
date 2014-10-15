#ifndef CODEGEN_GENIDENT_H
#define CODEGEN_GENIDENT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_is_value(compile_t* c, ast_t* left_type, ast_t* right_type,
  LLVMValueRef l_value, LLVMValueRef r_value);

LLVMValueRef gen_is(compile_t* c, ast_t* ast);

LLVMValueRef gen_isnt(compile_t* c, ast_t* ast);

PONY_EXTERN_C_END

#endif
