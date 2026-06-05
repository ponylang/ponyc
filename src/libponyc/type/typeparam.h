#ifndef TYPE_TYPEPARAM_H
#define TYPE_TYPEPARAM_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// The raw constraint. Returns null if unconstrained.
ast_t* typeparam_constraint(ast_t* typeparamref);

// Resolve a type-parameter definition (TK_TYPEPARAM) to the canonical root of
// its ast_data chain. Every TK_TYPEPARAM's data points one step toward the
// original definition set during the scope pass; copies — made via ast_dup or
// collect_type_params — retain that link, so a parameter bound through several
// layers (e.g. an iftype condition in a lambda inside a generic method) forms
// a chain that a single dereference would not fully resolve. Comparing two
// roots tests whether the parameters denote the same type variable. A null
// input or a null link stops the walk, so the result is null only when 'def'
// is.
ast_t* typeparam_root(ast_t* def);

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

/**
 * The constraint of the typeparam in the current scope.
 */
ast_t* typeparam_current(pass_opt_t* opt, ast_t* typeparamref, ast_t* scope);

PONY_EXTERN_C_END

#endif
