#ifndef EXPR_MATCH_H
#define EXPR_MATCH_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_match(ast_t* ast);
bool expr_cases(ast_t* ast);
bool expr_case(ast_t* ast);

PONY_EXTERN_C_END

#endif
