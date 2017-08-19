#ifndef CODEGEN_GENCALL_H
#define CODEGEN_GENCALL_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

void gen_send_message(compile_t* c, reach_method_t* m, LLVMValueRef args[],
  ast_t* args_ast);

LLVMValueRef gen_funptr(compile_t* c, ast_t* ast);

LLVMValueRef gen_call(compile_t* c, ast_t* ast);

LLVMValueRef gen_pattern_eq(compile_t* c, ast_t* pattern,
  LLVMValueRef r_value);

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast);

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gencall_create(compile_t* c, reach_type_t* t);

LLVMValueRef gencall_alloc(compile_t* c, reach_type_t* t);

LLVMValueRef gencall_allocstruct(compile_t* c, reach_type_t* t);

void gencall_throw(compile_t* c);

void gencall_memcpy(compile_t* c, LLVMValueRef dst, LLVMValueRef src,
  LLVMValueRef n);

void gencall_memmove(compile_t* c, LLVMValueRef dst, LLVMValueRef src,
  LLVMValueRef n);

void gencall_lifetime_start(compile_t* c, LLVMValueRef ptr);

void gencall_lifetime_end(compile_t* c, LLVMValueRef ptr);

PONY_EXTERN_C_END

#endif
