#ifndef SUBTYPE_H
#define SUBTYPE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool is_literal(ast_t* type, const char* name);

bool is_sub_cap_and_ephemeral(ast_t* sub, ast_t* super);

bool is_subtype(ast_t* sub, ast_t* super);

bool is_eqtype(ast_t* a, ast_t* b);

bool is_pointer(ast_t* type);

bool is_none(ast_t* type);

bool is_env(ast_t* type);

bool is_bool(ast_t* type);

bool is_float(ast_t* type);

bool is_integer(ast_t* type);

bool is_machine_word(ast_t* type);

bool is_signed(pass_opt_t* opt, ast_t* type);

// TODO(andy): This does not mean what the name implies to me. Add a comment
// stating what it means
bool is_composite(ast_t* type);

// TODO(andy): Similarly I have no idea what this is supposed to mean
bool is_constructable(ast_t* type);

bool is_concrete(ast_t* type);

bool is_known(ast_t* type);

bool is_actor(ast_t* type);

PONY_EXTERN_C_END

#endif
