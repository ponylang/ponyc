#ifndef EXPR_OPERATOR_H
#define EXPR_OPERATOR_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_identity(ast_t* ast);
bool expr_assign(pass_opt_t* opt, ast_t* ast);
bool expr_consume(typecheck_t* t, ast_t* ast);

PONY_EXTERN_C_END

#endif
