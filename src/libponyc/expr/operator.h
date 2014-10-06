#ifndef EXPR_OPERATOR_H
#define EXPR_OPERATOR_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_identity(ast_t* ast);
bool expr_assign(ast_t* ast);
bool expr_consume(ast_t* ast);
bool expr_recover(ast_t* ast);

PONY_EXTERN_C_END

#endif
