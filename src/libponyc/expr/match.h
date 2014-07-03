#ifndef EXPR_MATCH_H
#define EXPR_MATCH_H

#include "../ast/ast.h"

bool expr_match(ast_t* ast);
bool expr_cases(ast_t* ast);
bool expr_case(ast_t* ast);
bool expr_as(ast_t* ast);

#endif
