#ifndef TYPE_CAP_H
#define TYPE_CAP_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool is_cap_sub_cap(token_id sub, token_id super);

/**
 * Get the capability for a nominal type or type parameter reference.
 */
token_id cap_single(ast_t* type);

/**
 * The receiver capability is ref for constructors and behaviours. For
 * functions, it is defined by the function signature. A field initialiser is
 * considered part of a constructor.
 */
token_id cap_for_this(typecheck_t* t);

token_id cap_viewpoint(token_id view, token_id cap);

bool cap_sendable(token_id cap);

bool cap_safetowrite(token_id into, token_id cap);

token_id cap_from_constraint(ast_t* type);

PONY_EXTERN_C_END

#endif
