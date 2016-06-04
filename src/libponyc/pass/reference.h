#ifndef PASS_REFERENCE_H
#define PASS_REFERENCE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_reference(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
