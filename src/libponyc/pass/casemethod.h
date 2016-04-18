#ifndef PASS_CASEMETHOD_H
#define PASS_CASEMETHOD_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Sugar any case methods in the given entity.
ast_result_t sugar_case_methods(pass_opt_t* opt, ast_t* entity);

PONY_EXTERN_C_END

#endif
