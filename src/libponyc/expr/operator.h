#ifndef EXPR_OPERATOR_H
#define EXPR_OPERATOR_H

#include "../ast/ast.h"

bool expr_identity(ast_t* ast);
bool expr_compare(ast_t* ast);
bool expr_order(ast_t* ast);
bool expr_arithmetic(ast_t* ast);
bool expr_minus(ast_t* ast);
bool expr_shift(ast_t* ast);
bool expr_logical(ast_t* ast);
bool expr_not(ast_t* ast);
bool expr_assign(ast_t* ast);
bool expr_consume(ast_t* ast);
bool expr_recover(ast_t* ast);

#endif
