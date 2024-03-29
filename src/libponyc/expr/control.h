#ifndef EXPR_CONTROL_H
#define EXPR_CONTROL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool expr_seq(pass_opt_t* opt, ast_t* ast);
bool expr_if(pass_opt_t* opt, ast_t* ast);
bool expr_iftype(pass_opt_t* opt, ast_t* ast);
bool expr_while(pass_opt_t* opt, ast_t* ast);
bool expr_repeat(pass_opt_t* opt, ast_t* ast);
bool expr_try(pass_opt_t* opt, ast_t* ast);
bool expr_disposing_block(pass_opt_t* opt, ast_t* ast);
bool expr_recover(pass_opt_t* opt, ast_t* ast);
bool expr_break(pass_opt_t* opt, ast_t* ast);
bool expr_return(pass_opt_t* opt, ast_t* ast);
bool expr_error(pass_opt_t* opt, ast_t* ast);
bool expr_compile_error(pass_opt_t* opt, ast_t* ast);
bool expr_location(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
