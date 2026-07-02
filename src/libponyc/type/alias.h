#ifndef TYPE_ALIAS_H
#define TYPE_ALIAS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Alias a type in expression handling.
ast_t* alias(ast_t* type, pass_opt_t* opt);

ast_t* consume_type(ast_t* type, token_id cap, bool keep_double_ephemeral,
  pass_opt_t* opt);

ast_t* recover_type(ast_t* type, token_id cap, pass_opt_t* opt);

ast_t* chain_type(ast_t* type, token_id fun_cap, bool recovered_call,
  pass_opt_t* opt);

bool sendable(ast_t* type, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
