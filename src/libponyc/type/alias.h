#ifndef TYPE_ALIAS_H
#define TYPE_ALIAS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

// Alias a type in expression handling.
ast_t* alias(ast_t* type);

// Substitute bind capabilities for regular capabilities.
ast_t* bind_type(ast_t* type);

ast_t* consume_type(ast_t* type, token_id cap);

ast_t* recover_type(ast_t* type, token_id cap);

bool sendable(ast_t* type);

bool immutable_or_opaque(ast_t* type);

PONY_EXTERN_C_END

#endif
