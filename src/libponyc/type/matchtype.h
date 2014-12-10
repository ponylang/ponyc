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
 */
matchtype_t could_subtype(ast_t* sub, ast_t* super);

/**
 * Determine if a type contains an interface type. We can't determine if a
 * type is an interface type at runtime.
 */
bool contains_interface(ast_t* type);

PONY_EXTERN_C_END

#endif
