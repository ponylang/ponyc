#ifndef SUBTYPE_H
#define SUBTYPE_H

#include "../ast/ast.h"

bool is_subtype(ast_t* sub, ast_t* super);

bool is_eqtype(ast_t* a, ast_t* b);

bool is_literal(ast_t* type, const char* name);

bool is_builtin(ast_t* type, const char* name);

bool is_bool(ast_t* type);

bool is_arithmetic(ast_t* type);

bool is_integer(ast_t* type);

bool is_float(ast_t* type);

bool is_math_compatible(ast_t* a, ast_t* b);

bool is_id_compatible(ast_t* a, ast_t* b);

#endif
