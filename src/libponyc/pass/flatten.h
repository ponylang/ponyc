#ifndef PASS_FLATTEN_H
#define PASS_FLATTEN_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t flatten_typeparamref(pass_opt_t* opt, ast_t* ast);

ast_result_t pass_flatten(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
