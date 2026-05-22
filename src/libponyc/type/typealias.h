#ifndef TYPE_TYPEALIAS_H
#define TYPE_TYPEALIAS_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Unfold a TK_TYPEALIASREF until its head is no longer an alias.
 * Reifies the alias definition with the stored type arguments, applies
 * the cap and ephemeral modifiers, and follows chained aliases (e.g.,
 * `type A is B; type B is (U64, U64)`) so the returned head is always
 * a concrete type node (TK_NOMINAL, TK_UNIONTYPE, TK_TUPLETYPE, etc.)
 * — never another TK_TYPEALIASREF.
 *
 * Termination: PASS_TYPEALIAS_RECURSION rejects bare alias-only cycles,
 * and constructive recursion threads through a TK_NOMINAL typearg
 * position whose head isn't an alias, so the loop reaches a non-alias
 * head in finite steps for any input that passed legality. As a
 * defense against a legality-pass bug that lets an alias-only cycle
 * through, the implementation tracks visited alias defs and aborts
 * with a source-positioned compiler-internal-error if a cycle is
 * detected. May only be called after PASS_TYPEALIAS_RECURSION has run.
 *
 * Returns a newly allocated AST node, or NULL if reification fails.
 * Caller must free a non-NULL result with ast_free_unattached.
 */
ast_t* typealias_unfold(ast_t* typealiasref);

PONY_EXTERN_C_END

#endif
