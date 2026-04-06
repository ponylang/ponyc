#ifndef TYPE_TYPEALIAS_H
#define TYPE_TYPEALIAS_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Unfold a TK_TYPEALIASREF to its underlying type. Reifies the alias
 * definition with the stored type arguments and applies the cap and
 * ephemeral modifiers.
 *
 * Returns a newly allocated AST node, or NULL if reification fails.
 * The caller must free a non-NULL result with ast_free_unattached when done.
 */
ast_t* typealias_unfold(ast_t* typealiasref);

PONY_EXTERN_C_END

#endif
