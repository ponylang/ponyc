#ifndef COMPATTYPE_H
#define COMPATTYPE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Returns true if these types are locally compatible, false otherwise. This
 * does not examine subtyping at all: it checks purely for rcap local
 * compatibility.
 *
 * This is used to check for valid intesection types, since intersection types
 * represent two separate ways to view an object. Even though those views are
 * encapsulated in a single reference, the views must still be locally
 * compatible.
 */
bool is_compat_type(ast_t* a, ast_t* b);

/**
 * Returns true if the type could safely be the type of a variable.
 *
 * This is a descriptive synonym for is_compat_type(a, a).
 */
bool is_stable_type(ast_t* a);

PONY_EXTERN_C_END

#endif
