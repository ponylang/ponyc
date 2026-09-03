#ifndef EXPR_CALL_H
#define EXPR_CALL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool method_check_type_params(pass_opt_t* opt, ast_t** astp);
bool check_auto_recover_newref(ast_t* dest_type, ast_t* ast,
  pass_opt_t* opt);

// Extend positional arguments and apply named arguments for a call.
bool normalise_args(pass_opt_t* opt, ast_t* params, ast_t* positional,
  ast_t* namedargs);

// Apply inferred type arguments to a call's LHS (reify and replace).
bool apply_typeargs(pass_opt_t* opt, ast_t** astp, ast_t* typeargs,
  ast_t* inferred_from);

// Pre-order hook for TK_CALL: infer type arguments before child visiting.
ast_result_t expr_pre_call(pass_opt_t* opt, ast_t** astp);
bool expr_call(pass_opt_t* opt, ast_t** astp);

PONY_EXTERN_C_END

#endif
