#ifndef MATCHTYPE_H
#define MATCHTYPE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

typedef enum
{
  MATCHTYPE_ACCEPT,
  MATCHTYPE_REJECT,
  MATCHTYPE_DENY
} matchtype_t;

/**
 * Determine if we can pattern match on sub and extract a super.
 *
 * If a subtype of sub exists that is a subtype of super, return ACCEPT. If no
 * such subtype can exist, return REJECT. If a subtype can't exist, but one
 * could if capabilities were ignored, return DENY. This is to prevent a match
 * that could be detected with runtime information but would actually violate
 * the capability guarantees. When DENY is returned, no matching is allowed,
 * even if some other could_subtype() relationship exists.
 */
matchtype_t could_subtype(ast_t* sub, ast_t* super);

PONY_EXTERN_C_END

#endif
