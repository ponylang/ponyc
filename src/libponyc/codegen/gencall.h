#ifndef CODEGEN_GENCALL_H
#define CODEGEN_GENCALL_H

#include "codegen.h"

LLVMValueRef codegen_call(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, LLVMValueRef* args, int count, const char* ret);

LLVMValueRef codegen_callruntime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret);

#endif
