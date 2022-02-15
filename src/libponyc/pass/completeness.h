#ifndef PASS_COMPLETENESS_H
#define PASS_COMPLETENESS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_completeness(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
