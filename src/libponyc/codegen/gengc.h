#ifndef CODEGEN_GENGC_H
#define CODEGEN_GENGC_H

#include "codegen.h"

PONY_EXTERN_C_BEGIN

void gengc_free_liveness(struct gc_liveness_t* liveness);
bool gengc_contains_pointer(LLVMTypeRef type);
bool gengc_noncapturing_ffi(compile_t* c, const char* name);
bool gengc_borrowed_address(compile_t* c, ast_t* expr);
void gengc_pointer_result(compile_t* c, ast_t* expr, LLVMValueRef value);
bool gengc_method_eligible(compile_t* c, reach_type_t* t, reach_method_t* m);
bool gengc_allocating_expr(ast_t* expr);
bool gengc_poll(compile_t* c, ast_t* statement);

LLVMValueRef gengc_call(compile_t* c, ast_t* site, reach_type_t* t,
  reach_method_t* m, LLVMValueRef* args, unsigned n);

PONY_EXTERN_C_END

#endif
