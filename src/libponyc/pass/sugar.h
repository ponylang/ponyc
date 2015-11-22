#ifndef PASS_SUGAR_H
#define PASS_SUGAR_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Apply default receiver capability and return value, if needed, for the given
// function.
void fun_defaults(ast_t* ast);

ast_result_t pass_sugar(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
