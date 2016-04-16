#ifndef ID_INTERNAL_H
#define ID_INTERNAL_H

// This file exists purely to make the id checker visible to unit tests

#include <platform.h>
#include "ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// ID spec flags
#define START_UPPER               0x01
#define START_LOWER               0x02
#define ALLOW_LEADING_UNDERSCORE  0x04
#define ALLOW_UNDERSCORE          0x08
#define ALLOW_TICK                0x10


/* Check that the name in the given ID node meets the given spec.
 * If name is illegal an error will be generated.
 * The spec is supplied as a set of the above #defined flags.
 */
 bool check_id(pass_opt_t* opt, ast_t* id_node, const char* desc, int spec);

PONY_EXTERN_C_END

#endif
