#ifndef EXPR_OPERATOR_H
#define EXPR_OPERATOR_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_identity(ast_t* ast);
bool expr_compare(ast_t** astp);
bool expr_order(ast_t** astp);
bool expr_arithmetic(ast_t** astp);
bool expr_minus(ast_t* ast);
bool expr_shift(ast_t* ast);
bool expr_and(ast_t** astp);
bool expr_or(ast_t** astp);
bool expr_logical(ast_t* ast);
bool expr_not(ast_t* ast);
bool expr_assign(ast_t* ast);
bool expr_consume(ast_t* ast);
bool expr_recover(ast_t* ast);

PONY_EXTERN_C_END

#endif
