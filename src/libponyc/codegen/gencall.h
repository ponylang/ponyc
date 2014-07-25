#ifndef CODEGEN_GENCALL_H
#define CODEGEN_GENCALL_H

#include "codegen.h"

LLVMValueRef gencall(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gencall_alloc(compile_t* c, LLVMTypeRef type);

void gencall_tracetag(compile_t* c, LLVMValueRef field);

void gencall_traceactor(compile_t* c, LLVMValueRef field);

void gencall_traceknown(compile_t* c, LLVMValueRef field, const char* name);

void gencall_traceunknown(compile_t* c, LLVMValueRef field);

#endif
