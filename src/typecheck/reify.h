#ifndef REIFY_H
#define REIFY_H

#include "../ast/ast.h"

ast_t* reify_typeparams(ast_t* typeparams, ast_t* typeargs);

ast_t* reify_type(ast_t* type, ast_t* typeparams, ast_t* typeargs);

#endif
