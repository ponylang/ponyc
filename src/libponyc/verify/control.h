#ifndef VERIFY_CONTROL_H
#define VERIFY_CONTROL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool verify_try(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
