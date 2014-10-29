#ifndef CODEGEN_GENCALL_H
#define CODEGEN_GENCALL_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_call(compile_t* c, ast_t* ast);

LLVMValueRef gen_pattern_eq(compile_t* c, ast_t* pattern, LLVMValueRef r_value);

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast);

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gencall_create(compile_t* c, gentype_t* g);

LLVMValueRef gencall_alloc(compile_t* c, gentype_t* g);

LLVMValueRef gencall_allocstruct(compile_t* c, gentype_t* g);

bool gencall_trace(compile_t* c, LLVMValueRef value, ast_t* type);

void gencall_throw(compile_t* c);

PONY_EXTERN_C_END

#endif
