#ifndef EXPR_POSTFIX_H
#define EXPR_POSTFIX_H

#include "../ast/ast.h"

bool expr_qualify(ast_t* ast);
bool expr_dot(ast_t* ast);
bool expr_call(ast_t* ast);
bool expr_ffi(ast_t* ast);

#endif
