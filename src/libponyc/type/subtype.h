#ifndef SUBTYPE_H
#define SUBTYPE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool is_subtype(ast_t* sub, ast_t* super);

bool is_eqtype(ast_t* a, ast_t* b);

bool is_pointer(ast_t* type);

bool is_none(ast_t* type);

bool is_env(ast_t* type);

bool is_bool(ast_t* type);

bool is_machine_word(ast_t* type);

bool is_signed(pass_opt_t* opt, ast_t* type);

bool is_constructable(ast_t* type);

bool is_concrete(ast_t* type);

PONY_EXTERN_C_END

#endif
