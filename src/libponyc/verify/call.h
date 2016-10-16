#ifndef VERIFY_CALL_H
#define VERIFY_CALL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool verify_function_call(pass_opt_t* opt, ast_t* ast);
bool verify_behaviour_call(pass_opt_t* opt, ast_t* ast);
bool verify_ffi_call(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
