#ifndef EXPR_REFERENCE_H
#define EXPR_REFERENCE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_field(pass_opt_t* opt, ast_t* ast);
bool expr_fieldref(ast_t* ast, ast_t* find, token_id t);
bool expr_typeref(pass_opt_t* opt, ast_t* ast);
bool expr_reference(pass_opt_t* opt, ast_t* ast);
bool expr_local(typecheck_t* t, ast_t* ast);
bool expr_idseq(ast_t* ast);
bool expr_addressof(ast_t* ast);
bool expr_dontcare(ast_t* ast);
bool expr_this(typecheck_t* t, ast_t* ast);
bool expr_tuple(ast_t* ast);
bool expr_nominal(typecheck_t* t, ast_t** astp);
bool expr_fun(pass_opt_t* opt, ast_t* ast);
bool expr_compiler_intrinsic(typecheck_t* t, ast_t* ast);

PONY_EXTERN_C_END

#endif
