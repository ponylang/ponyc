#ifndef PASS_EXPR_H
#define PASS_EXPR_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool is_result_needed(ast_t* ast);

bool is_method_result(typecheck_t* t, ast_t* ast);

bool is_method_return(typecheck_t* t, ast_t* ast);

bool is_typecheck_error(ast_t* type);

ast_result_t pass_expr(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
