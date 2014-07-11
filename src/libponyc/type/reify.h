#ifndef REIFY_H
#define REIFY_H

#include "../ast/ast.h"

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs);

bool check_constraints(ast_t* scope, ast_t* typeparams, ast_t* typeargs);

#endif
