#ifndef EXPR_ARRAY_H
#define EXPR_ARRAY_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t expr_pre_array(pass_opt_t* opt, ast_t** astp);

bool expr_array(pass_opt_t* opt, ast_t** astp);

PONY_EXTERN_C_END

#endif
