#ifndef SUBTYPE_H
#define SUBTYPE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool is_subtype(ast_t* sub, ast_t* super);

bool is_eqtype(ast_t* a, ast_t* b);

bool is_none(ast_t* type);

bool is_bool(ast_t* type);

bool is_signed(ast_t* type);

PONY_EXTERN_C_END

#endif
