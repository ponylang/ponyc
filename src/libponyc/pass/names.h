#ifndef PASS_NAMES_H
#define PASS_NAMES_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_t* names_def(ast_t* ast);

bool names_nominal(pass_opt_t* opt, ast_t* scope, ast_t** astp);

ast_result_t pass_names(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
