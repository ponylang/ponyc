#ifndef EXPR_LITERAL_H
#define EXPR_LITERAL_H

#include "../ast/ast.h"

bool expr_literal(ast_t* ast, const char* name);
bool expr_this(ast_t* ast);
bool expr_tuple(ast_t* ast);
bool expr_error(ast_t* ast);
bool expr_compiler_intrinsic(ast_t* ast);
bool expr_fun(ast_t* ast);

#endif
