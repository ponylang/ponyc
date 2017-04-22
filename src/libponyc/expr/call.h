#ifndef EXPR_CALL_H
#define EXPR_CALL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool method_check_type_params(pass_opt_t* opt, ast_t** astp);

bool expr_call(pass_opt_t* opt, ast_t** astp);

PONY_EXTERN_C_END

#endif
