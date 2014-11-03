#ifndef TYPE_ALIAS_H
#define TYPE_ALIAS_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

// Alias a type in expression handling.
ast_t* alias(ast_t* type);

// Alias a type when binding it.
ast_t* alias_bind(ast_t* type);

ast_t* consume_type(ast_t* type);

ast_t* recover_type(ast_t* type);

ast_t* send_type(ast_t* type);

bool sendable(ast_t* type);

PONY_EXTERN_C_END

#endif
