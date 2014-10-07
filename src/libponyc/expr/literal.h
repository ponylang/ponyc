#ifndef EXPR_LITERAL_H
#define EXPR_LITERAL_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_literal(ast_t* ast, const char* name);
bool expr_this(ast_t* ast);
bool expr_tuple(ast_t* ast);
bool expr_error(ast_t* ast);
bool expr_compiler_intrinsic(ast_t* ast);
bool expr_nominal(ast_t** astp);
bool expr_fun(ast_t* ast);

bool is_type_arith_literal(ast_t* ast);
bool is_literal_subtype(token_id literal_id, ast_t* target);
bool coerce_literals(ast_t* ast, ast_t* target_type);

PONY_EXTERN_C_END

#endif
