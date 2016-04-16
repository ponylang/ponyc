#ifndef EXPR_MATCH_H
#define EXPR_MATCH_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool expr_match(pass_opt_t* opt, ast_t* ast);
bool expr_cases(pass_opt_t* opt, ast_t* ast);
bool expr_case(pass_opt_t* opt, ast_t* ast);
bool expr_match_capture(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
