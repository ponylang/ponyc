#ifndef TYPE_TYPEPARAM_H
#define TYPE_TYPEPARAM_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

// The raw constraint. Returns null if unconstrained.
ast_t* typeparam_constraint(ast_t* typeparamref);

/**
 * If the upper bounds of a typeparamref is a subtype of a type T, then every
 * possible binding of the typeparamref is a subtype of T.
 */
ast_t* typeparam_upper(ast_t* typeparamref);

/**
 * If a type T is a subtype of the lower bounds of a typeparamref, it is
 * always a subtype of the typeparamref. Returns null if there is no lower
 * bounds.
 */
ast_t* typeparam_lower(ast_t* typeparamref);

/**
 * Set the refcap of a typeparamref to be the set of all refcaps that could
 * be bound.
 */
void typeparam_set_cap(ast_t* typeparamref);

PONY_EXTERN_C_END

#endif
