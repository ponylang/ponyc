#ifndef EXPR_LAMBDA_H
#define EXPR_LAMBDA_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_lambda(pass_opt_t* opt, ast_t** astp);

PONY_EXTERN_C_END

#endif
