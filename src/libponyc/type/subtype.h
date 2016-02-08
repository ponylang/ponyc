#ifndef SUBTYPE_H
#define SUBTYPE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/error.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool is_literal(ast_t* type, const char* name);

bool is_sub_cap_and_ephemeral(ast_t* sub, ast_t* super);

bool is_subtype(ast_t* sub, ast_t* super, errorframe_t* errors);

bool is_eqtype(ast_t* a, ast_t* b, errorframe_t* errors);

bool is_pointer(ast_t* type);

bool is_maybe(ast_t* type);

bool is_none(ast_t* type);

bool is_env(ast_t* type);

bool is_bool(ast_t* type);

bool is_float(ast_t* type);

bool is_integer(ast_t* type);

bool is_machine_word(ast_t* type);

bool is_signed(pass_opt_t* opt, ast_t* type);

bool is_constructable(ast_t* type);

bool is_concrete(ast_t* type);

bool is_known(ast_t* type);

bool is_entity(ast_t* type, token_id entity);

bool contains_dontcare(ast_t* ast);

PONY_EXTERN_C_END

#endif
