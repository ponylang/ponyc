#ifndef EXPR_FFI_H
#define EXPR_FFI_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t expr_ffi(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
