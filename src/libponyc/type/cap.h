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
token_id cap_for_this(typecheck_t* t, ast_t* ast);

/**
 * Gets the capability for a type. Upper bounds of union and arrow types, lower
 * bounds of isect types. Fails on tuple types.
 */
token_id cap_for_type(ast_t* type);

token_id cap_viewpoint(token_id view, token_id cap);

/**
 * Given the capability of a constraint, derive the capability to use for a
 * typeparamref of that constraint. Returns the lowest capability that could
 * be a subtype of the constraint.
 */
token_id cap_typeparam(token_id cap);

bool cap_sendable(token_id cap);

bool cap_safetowrite(token_id into, token_id cap);

PONY_EXTERN_C_END

#endif
