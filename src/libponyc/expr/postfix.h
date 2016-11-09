#ifndef EXPR_POSTFIX_H
#define EXPR_POSTFIX_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool expr_qualify(pass_opt_t* opt, ast_t** astp);
bool expr_dot(pass_opt_t* opt, ast_t** astp);
bool expr_tilde(pass_opt_t* opt, ast_t** astp);
bool expr_chain(pass_opt_t* opt, ast_t** astp);

PONY_EXTERN_C_END

#endif
