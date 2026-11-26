#ifndef CODEGEN_GENGC_H
#define CODEGEN_GENGC_H

#include "codegen.h"

PONY_EXTERN_C_BEGIN

void gengc_free_liveness(struct gc_liveness_t* liveness);
bool gengc_contains_pointer(LLVMTypeRef type);
bool gengc_noncapturing_ffi(compile_t* c, const char* name);
bool gengc_borrowed_address(compile_t* c, ast_t* expr);
void gengc_pointer_result(compile_t* c, ast_t* expr, LLVMValueRef value);

// True if m gets a collecting copy: m is a `fun` that may allocate and isn't
// excluded from collecting copies, and it has a loop, is recursive, or calls a
// method that gets a collecting copy.
bool gengc_method_has_copy(compile_t* c, reach_type_t* t, reach_method_t* m);

// True if m is in a cycle of recorded calls between methods that get
// collecting copies. Valid once gengc_method_has_copy has returned true for m.
bool gengc_method_recursive(reach_method_t* m);

// True if the current method's reach data records a call to (t, m).
bool gengc_call_recorded(compile_t* c, reach_type_t* t, reach_method_t* m);

// Polls at the top of a `while` or `repeat` body. Returns false, and emits
// nothing, if the current function doesn't collect or the roots there can't be
// traced.
bool gengc_poll(compile_t* c, ast_t* loop);

// Starts generating the body of m's collecting copy. If m is recursive, polls
// before the first statement of body; if the roots there can't be traced, no
// call in body runs a collecting copy.
void gengc_start_copy(compile_t* c, reach_method_t* m, ast_t* body);

// Polls before a recursive collecting copy returns value, so that the garbage
// it made after its last poll can be collected while the recursion unwinds.
// value is the value of expr, has the type `type` (the method's declared
// result type when type is NULL), and isn't wrapped for a partial function.
// May emit nothing.
void gengc_exit_poll(compile_t* c, LLVMValueRef value, ast_t* expr,
  ast_t* type);

// Polls before a recursive collecting copy returns an error. Emits nothing in
// other functions.
void gengc_error_exit_poll(compile_t* c);

// True if construct, an `if` or `match`, should poll at the end of each
// branch: it is in a recursive collecting copy, its value is needed, and it is
// the value the copy returns at the end of its body. An `if` or `match` under
// `return` doesn't poll per branch.
bool gengc_polls_branches(compile_t* c, ast_t* construct);

LLVMValueRef gengc_call(compile_t* c, ast_t* site, reach_type_t* t,
  reach_method_t* m, LLVMValueRef* args, unsigned n);

PONY_EXTERN_C_END

#endif
