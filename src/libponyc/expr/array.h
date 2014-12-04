#ifndef EXPR_ARRAY_H
#define EXPR_ARRAY_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_array(typecheck_t* t, ast_t** astp);

PONY_EXTERN_C_END

#endif
