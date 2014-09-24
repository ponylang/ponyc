#ifndef PASS_EXPR_H
#define PASS_EXPR_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_expr(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
