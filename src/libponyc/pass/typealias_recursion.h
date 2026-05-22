#ifndef PASS_TYPEALIAS_RECURSION_H
#define PASS_TYPEALIAS_RECURSION_H

#include <platform.h>
#include "../ast/ast.h"
#include "pass.h"

PONY_EXTERN_C_BEGIN

/**
 * Whole-program legality check for recursive type aliases.
 *
 * Walks the alias-reference graph across all packages, computes its
 * strongly-connected components, and rejects every cycle that doesn't
 * meet two productivity conditions:
 *
 * 1. At least one edge in the SCC must be constructive — reaching
 *    another alias in the SCC through a TK_NOMINAL's typeargs. The
 *    nominal is the constructor; the typearg lives inside an instance
 *    of it. Without a constructive edge, every recursive reference is
 *    bare self-reference and choosing the recursive branch never adds
 *    structure.
 *
 * 2. At least one alias in the SCC must have a TK_UNIONTYPE (at any
 *    depth in its body) where some member transitively references no
 *    alias in the SCC. That non-recursive union member is the base
 *    case that lets a value be constructed at finite depth.
 *
 * Tuple elements do not count as constructive even though a tuple is
 * a value constructor. Pony tuples are inline value types: a tuple's
 * layout inlines each element, so a tuple element that recurses back
 * to its containing alias would require the alias's layout to inline
 * itself transitively, which has no finite size. Nominal classes
 * break the recursion at the heap pointer (the class's own layout is
 * fixed; recursive typeargs live behind the pointer), which is why
 * recursion through a class's typearg is sound and recursion through
 * a tuple element is not.
 *
 * Type-parameter constraints (D4) and the LHS of TK_ARROW (D5) are
 * excluded from the reference graph because they aren't part of an
 * alias's value structure.
 *
 * Examples:
 *
 *   LEGAL — recursion through a nominal typearg with a union base case:
 *     type JsonValue is (String | F64 | Bool | None | JsonArray)
 *     type JsonArray is Array[JsonValue]
 *
 *   LEGAL — mutual recursion through nominal typeargs:
 *     type B is Array[C]
 *     type C is (Array[B] | None)
 *
 *   ILLEGAL — bare self-reference inside a union (no constructor):
 *     type A is (A | None)
 *
 *   ILLEGAL — mutual cycle through union members only:
 *     type X is (Y | String)
 *     type Y is (X | U32)
 *
 *   ILLEGAL — recursion through a tuple element (tuples inline their
 *   elements, so this has no finite layout):
 *     type IntList is (None | (U32, IntList))
 *
 *   ILLEGAL — constructive edge but no base case:
 *     type A is Array[A]
 *
 * Errors are reported via ast_error / ast_error_frame. Returns AST_OK
 * if legal, AST_FATAL otherwise — no later pass can run safely on an
 * AST containing an illegal recursive alias.
 *
 * Invoked once at the program level after PASS_NAME_RESOLUTION (so
 * every TK_TYPEALIASREF has its definition pointer set) and before
 * PASS_FLATTEN.
 *
 * Multi-package programs are handled implicitly: collect_aliases
 * walks every module in every package, and node_for_def keys by the
 * TK_TYPE def pointer (which is unique per definition regardless of
 * package). An SCC with members in different packages would still be
 * detected, but Pony's package system forbids circular imports — two
 * packages can't both 'use' each other — so such an SCC isn't
 * expressible. A single package containing a multi-alias SCC, or a
 * recursive alias in one package consumed from another, both work.
 *
 * For sub-AST catch-up calls where '*astp' is not TK_PROGRAM, returns
 * AST_OK without analysis: aliases live at the package level and are
 * checked for the program as a whole.
 */
ast_result_t pass_typealias_recursion(ast_t** astp, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
