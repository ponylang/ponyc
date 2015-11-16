#ifndef EXPR_CONTROL_H
#define EXPR_CONTROL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool expr_seq(pass_opt_t* opt, ast_t* ast);
bool expr_if(pass_opt_t* opt, ast_t* ast);
bool expr_while(pass_opt_t* opt, ast_t* ast);
bool expr_repeat(pass_opt_t* opt, ast_t* ast);
bool expr_try(pass_opt_t* opt, ast_t* ast);
bool expr_recover(ast_t* ast);
bool expr_break(typecheck_t* t, ast_t* ast);
bool expr_continue(typecheck_t* t, ast_t* ast);
bool expr_return(pass_opt_t* opt, ast_t* ast);
bool expr_error(ast_t* ast);
bool expr_compile_error(ast_t* ast);

PONY_EXTERN_C_END

#endif
