#ifndef IFDEF_H
#define IFDEF_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Normalise the given ifdef condition to canonical form.
// Also used for use command guards.
// Includes replacing TK_*s with ifdef specific versions.
// Returns: true on success, false if condition can nver be true (does not
// report error for this).
bool ifdef_cond_normalise(ast_t** astp, pass_opt_t* opt);

// Evaluate the given ifdef or use guard condition for our build target.
// Returns: value of given condition.
bool ifdef_cond_eval(ast_t* ast, pass_opt_t* opt);

// Find the FFI declaration for the given FFI call, if any.
// Returns: true on success, false on failure.
// On success out_decl contains the relevant declaration, or NULL if there
// isn't one.
bool ffi_get_decl(typecheck_t* t, ast_t* ast, ast_t** out_decl,
  pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
