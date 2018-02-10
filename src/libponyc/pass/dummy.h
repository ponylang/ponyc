#ifndef PASS_DUMMY_H
#define PASS_DUMMY_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_pre_dummy(ast_t** astp, pass_opt_t* options);
ast_result_t pass_dummy(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
