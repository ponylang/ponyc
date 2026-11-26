#ifndef CODEGEN_GENCALL_H
#define CODEGEN_GENCALL_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

void gen_send_message(compile_t* c, reach_method_t* m, LLVMValueRef args[],
  ast_t* args_ast);

LLVMValueRef gen_funptr(compile_t* c, ast_t* ast);

// True if gen_call compiles this call to inline instructions instead of a
// method call.
bool gencall_is_inline(pass_opt_t* opt, deferred_reification_t* reify,
  ast_t* call);

// True if this call, which resolves to method m on a receiver of type t, is
// of a kind gen_call can route to a collecting copy or a collecting dispatch.
// Doesn't exclude calls gen_call compiles inline; see gencall_is_inline.
bool gencall_gc_routable(ast_t* call, reach_type_t* t, reach_method_t* m);

// True if t is a class, actor or primitive.
bool gencall_is_concrete(reach_type_t* t);

// The method of the concrete type sub that a collecting dispatch for the
// abstract method m would call directly, or NULL if there is none. Doesn't
// check whether that method gets a collecting copy.
reach_method_t* gencall_gc_concrete(compile_t* c, reach_type_t* sub,
  reach_method_t* m);

LLVMValueRef gencall_gc_dispatch(compile_t* c, reach_type_t* t,
  reach_method_t* m, bool loop_polls);

LLVMValueRef gen_call(compile_t* c, ast_t* ast);

LLVMValueRef gen_pattern_eq(compile_t* c, ast_t* pattern,
  LLVMValueRef r_value, bool emit_ror);

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast);

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret);

LLVMValueRef gencall_create(compile_t* c, reach_type_t* t, ast_t* call);

LLVMValueRef gencall_alloc(compile_t* c, reach_type_t* t, ast_t* call);

LLVMValueRef gencall_allocstruct(compile_t* c, reach_type_t* t);

LLVMTypeRef error_flag_type(compile_t* c, LLVMTypeRef real_type);

LLVMValueRef wrap_result(compile_t* c, LLVMValueRef value,
  LLVMValueRef is_error);

LLVMValueRef unwrap_result(compile_t* c, LLVMValueRef tuple);

LLVMValueRef unwrap_error(compile_t* c, LLVMValueRef tuple);

void gencall_memcpy(compile_t* c, LLVMValueRef dst, LLVMValueRef src,
  LLVMValueRef n);

void gencall_memmove(compile_t* c, LLVMValueRef dst, LLVMValueRef src,
  LLVMValueRef n);

void gencall_lifetime_start(compile_t* c, LLVMValueRef ptr);

void gencall_lifetime_end(compile_t* c, LLVMValueRef ptr);

PONY_EXTERN_C_END

#endif
