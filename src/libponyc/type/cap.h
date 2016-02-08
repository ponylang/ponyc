#ifndef TYPE_CAP_H
#define TYPE_CAP_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

/**
 * Every possible instantiation of sub is a subtype of every possible
 * instantiation of super.
 */
bool is_cap_sub_cap(token_id sub, token_id subalias, token_id super,
  token_id supalias);

/**
 * Some possible instantiation of the operand is a subtype of some possible
 * instantiation of the pattern.
 */
bool is_cap_match_cap(token_id operand_cap, token_id operand_eph,
  token_id pattern_cap, token_id pattern_eph);

/**
 * Every possible instantiation of the left side is locally compatible with
 * every possible instantiation of the right side. This relationship is
 * symmetric.
 */
bool is_cap_compat_cap(token_id left_cap, token_id left_eph,
  token_id right_cap, token_id right_eph);

/**
 * Get the capability ast for a nominal type or type parameter reference.
 */
ast_t* cap_fetch(ast_t* type);

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

token_id cap_bind(token_id cap);

token_id cap_unbind(token_id cap);

/**
 * Returns the upper bounds of left->right.
 */
bool cap_view_upper(token_id left_cap, token_id left_eph,
  token_id* right_cap, token_id* right_eph);

/**
 * Returns the lower bounds of left->right.
 */
bool cap_view_lower(token_id left_cap, token_id left_eph,
  token_id* right_cap, token_id* right_eph);

bool cap_sendable(token_id cap);

bool cap_safetowrite(token_id into, token_id cap);

PONY_EXTERN_C_END

#endif
