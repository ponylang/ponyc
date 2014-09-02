#ifndef CODEGEN_GENCALL_H
#define CODEGEN_GENCALL_H

#include "codegen.h"
#include "gentype.h"

LLVMValueRef gencall(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gen_call(compile_t* c, ast_t* ast);

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gencall_create(compile_t* c, gentype_t* g);

LLVMValueRef gencall_alloc(compile_t* c, LLVMTypeRef type);

bool gencall_trace(compile_t* c, LLVMValueRef value, ast_t* type);

#endif
