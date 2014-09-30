#ifndef SUBTYPE_H
#define SUBTYPE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool is_subtype(ast_t* sub, ast_t* super);

bool is_eqtype(ast_t* a, ast_t* b);

bool is_literal(ast_t* type, const char* name);

bool is_builtin(ast_t* type, const char* name);

bool is_bool(ast_t* type);

bool is_sintliteral(ast_t* type);

bool is_uintliteral(ast_t* type);

bool is_intliteral(ast_t* type);

bool is_floatliteral(ast_t* type);

bool is_arithmetic(ast_t* type);

bool is_integer(ast_t* type);

bool is_float(ast_t* type);

bool is_signed(ast_t* type);

bool is_singletype(ast_t* type);

bool is_math_compatible(ast_t* a, ast_t* b);

bool is_id_compatible(ast_t* a, ast_t* b);

bool is_match_compatible(ast_t* match, ast_t* pattern);

PONY_EXTERN_C_END

#endif
