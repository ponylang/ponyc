#ifndef TYPE_TYPEALIAS_H
#define TYPE_TYPEALIAS_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Unfold a TK_TYPEALIASREF to its underlying type. Reifies the alias
 * definition with the stored type arguments and applies the cap and
 * ephemeral modifiers. If the result is itself a TK_TYPEALIASREF (a chained
 * alias), unfolds transitively until the head is a concrete type.
 *
 * Guarantees that a non-NULL result has a head that is not TK_TYPEALIASREF.
 * Callers dispatching on the head of the result can therefore rely on the
 * full set of non-alias heads (TK_NOMINAL, TK_UNIONTYPE, TK_ISECTTYPE,
 * TK_TUPLETYPE, TK_TYPEPARAMREF, TK_ARROW, etc.) without a TK_TYPEALIASREF
 * case. Nested aliases inside a compound type (a member of a TK_UNIONTYPE,
 * TK_ISECTTYPE, or TK_TUPLETYPE that is itself an alias) are not unfolded;
 * callers that recurse into compound types must still dispatch on
 * TK_TYPEALIASREF for interior positions.
 *
 * Returns a newly allocated AST node, or NULL if reification fails.
 * The caller must free a non-NULL result with ast_free_unattached when done.
 */
ast_t* typealias_unfold(ast_t* typealiasref);

PONY_EXTERN_C_END

#endif
