#ifndef EXPR_REFERENCE_H
#define EXPR_REFERENCE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_field(ast_t* ast);
bool expr_fieldref(ast_t* ast, ast_t* left, ast_t* find, token_id t);
bool expr_typeref(typecheck_t* t, ast_t* ast);
bool expr_reference(typecheck_t* t, ast_t* ast);
bool expr_local(typecheck_t* t, ast_t* ast);
bool expr_idseq(ast_t* ast);
bool expr_addressof(ast_t* ast);
bool expr_dontcare(ast_t* ast);

PONY_EXTERN_C_END

#endif
