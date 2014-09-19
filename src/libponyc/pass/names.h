#ifndef PASS_NAMES_H
#define PASS_NAMES_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool names_nominal(ast_t* scope, ast_t** astp);

ast_result_t pass_names(ast_t** astp);

ast_result_t pass_flatten(ast_t** astp);

PONY_EXTERN_C_END

#endif
