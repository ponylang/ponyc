#ifndef CODEGEN_GENTRACE_H
#define CODEGEN_GENTRACE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

bool gentrace_needed(compile_t* c, ast_t* src_type, ast_t* dst_type);

void gentrace_prototype(compile_t* c, reach_type_t* t);

void gentrace(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* src_type, ast_t* dst_type);

PONY_EXTERN_C_END

#endif
