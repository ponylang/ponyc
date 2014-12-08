#ifndef EXPR_CALL_H
#define EXPR_CALL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_call(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
