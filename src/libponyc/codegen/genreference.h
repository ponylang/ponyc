#ifndef CODEGEN_GENLITERAL_H
#define CODEGEN_GENLITERAL_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_this(compile_t* c, ast_t* ast);

LLVMValueRef gen_param(compile_t* c, ast_t* ast);

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast);

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast);

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast);

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast);

LLVMValueRef gen_localptr(compile_t* c, ast_t* ast);

LLVMValueRef gen_localload(compile_t* c, ast_t* ast);

LLVMValueRef gen_addressof(compile_t* c, ast_t* ast);

LLVMValueRef gen_identity(compile_t* c, ast_t* ast);

LLVMValueRef gen_int(compile_t* c, ast_t* ast);

LLVMValueRef gen_float(compile_t* c, ast_t* ast);

LLVMValueRef gen_string(compile_t* c, ast_t* ast);

PONY_EXTERN_C_END

#endif
