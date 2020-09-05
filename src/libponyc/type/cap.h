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
 * Every possible instantiation of sub is a member of the set of every possible
 * instantiation of super.
 */
bool is_cap_sub_cap_constraint(token_id sub, token_id subalias, token_id super,
  token_id supalias);

/**
 * Check subtyping when matching type params.
 * When comparing rcap constraints, this uses the knowledge that
 * the constraint is altered from the original.
 *
 * If either rcap is a specific/singular capability, use the same rule
 * as in is_cap_sub_cap: every possible instantiation of sub must be a
 * subtype of every possible instantiation of super.
 *
 * If both rcaps are generic/set capabilities, use the following rule:
 * every instantiation of the super rcap must be a supertype of some
 * instantiation of the sub rcap (but not necessarily the same instantiation
 */
bool is_cap_sub_cap_bound(token_id sub, token_id subalias, token_id super,
  token_id supalias);

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
 * Get the capability that should be used for dispatch.
 */
token_id cap_dispatch(ast_t* type);

/**
 * The receiver capability is ref for constructors and behaviours. For
 * functions, it is defined by the function signature. A field initialiser is
 * considered part of a constructor.
 */
token_id cap_for_this(typecheck_t* t);

typedef enum {
  WRITE,
  EXTRACT
} direction;

bool cap_safetomove(token_id into, token_id cap, direction direction);

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

bool cap_immutable_or_opaque(token_id cap);

/**
 * For a temporary capability (iso/trn), returns
 * the capability with no isolation constraints
 */
token_id cap_unisolated(token_id cap);

ast_t* unisolated(ast_t* type);

/**
 * Reads/writes the given capability/ephemeral values
 * and returns true if they were changed
 */
typedef bool cap_mutation(token_id* cap, token_id* eph);
/**
 * Returns a type whose outermost capability/ies have
 * been modified by the given function.
 *
 * In the case of arrows, this modifies each component
 */
ast_t* modified_cap(ast_t* type, cap_mutation* mut);

PONY_EXTERN_C_END


#endif
