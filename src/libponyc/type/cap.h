#ifndef TYPE_CAP_H
#define TYPE_CAP_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool is_cap_sub_cap(token_id sub, token_id super);

/**
 * The receiver capability is ref for constructors and behaviours. For
 * functions, it is defined by the function signature. A field initialiser is
 * considered part of a constructor.
 */
token_id cap_for_receiver(ast_t* ast);

token_id cap_for_type(ast_t* type);

token_id cap_viewpoint(token_id view, token_id cap);

token_id cap_viewpoint_lower(token_id cap);

token_id cap_alias(token_id cap);

token_id cap_recover(token_id cap);

token_id cap_send(token_id cap);

bool cap_sendable(token_id cap);

bool cap_safetowrite(token_id into, token_id cap);

bool cap_compatible(ast_t* a, ast_t* b);

PONY_EXTERN_C_END

#endif
