#ifndef EXPR_FFI_H
#define EXPR_FFI_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool expr_ffi(pass_opt_t* opt, ast_t* ast);
bool void_star_param(ast_t* param_type, ast_t* arg_type);

PONY_EXTERN_C_END

#endif
