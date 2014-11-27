#ifndef EXPR_POSTFIX_H
#define EXPR_POSTFIX_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_qualify(typecheck_t* t, ast_t* ast);
bool expr_dot(typecheck_t* t, ast_t* ast);
bool expr_call(typecheck_t* t, ast_t* ast);
bool expr_ffi(ast_t* ast);

PONY_EXTERN_C_END

#endif
