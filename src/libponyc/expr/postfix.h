#ifndef EXPR_POSTFIX_H
#define EXPR_POSTFIX_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_qualify(ast_t* ast);
bool expr_dot(ast_t* ast);
bool expr_call(ast_t* ast);
bool expr_ffi(ast_t* ast);

PONY_EXTERN_C_END

#endif
