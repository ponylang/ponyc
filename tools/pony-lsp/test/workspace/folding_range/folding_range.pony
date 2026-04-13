"""
Test fixtures for exercising LSP textDocument/foldingRange functionality.

A fold range is emitted for each multi-line top-level entity (class, actor,
struct, primitive, trait, interface, type alias), each multi-line member
(fun, be, new), and each multi-line compound expression within a method body
(if, ifdef, while, for, repeat, match, try, object, lambda, recover).
Single-line nodes are excluded since there is nothing to fold.

Note: `with` desugars to `try_no_check` before typechecking, so fold ranges
for `with` expressions are produced via the try arm rather than a dedicated
`with` arm.

To manually test folding range functionality:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Open the files in the folding_range directory while the Pony language
   server is active.
3. Verify that fold indicators appear in the gutter at the expected lines and
   that collapsing each region hides the correct content.

Expected fold regions per file:

  foldable.pony — class body and multi-line members; single-line excluded
    - `class Foldable` (line 1) → folds the entire class body
    - `new create(x: U32) =>` (line 7) → folds through `_x = x`
    - `fun get_value(): U32 =>` (line 10) → folds through `_x`
    - `fun single_line(): U32 => _x` — single-line, no fold indicator

  expressions.pony — class, methods, and expression-level ranges
    - `class Expressions` (line 1) → folds entire class
    - `fun with_if` (line 6) → folds method body
    - `if x > 0 then` (line 7) → folds through `0` (the else branch)
    - `fun with_match` (line 13) → folds method body
    - `match x` (line 14) → folds through `"many"`
    - `fun with_while` (line 21) → folds method body
    - `while i < x do` (line 23) → folds through `i = i + 1`
    - `fun with_try` (line 28) → folds method body
    - `try` (line 29) → folds through `0` (the else branch)

  more_expressions.pony — for, repeat, with, object, lambda, recover
    - `class MoreExpressions` (line 1) → folds entire class
    - `fun with_for` (line 6) → folds method body
    - `for i in ...` (line 8) → folds through `sum = sum + i`
    - `fun with_repeat` (line 13) → folds method body
    - `repeat` (line 15) → folds through `i >= 3` (the until condition)
    - `fun with_with` (line 20) → folds method body
      (`with` desugars to try — no separate expression-level fold)
    - `fun with_object` (line 25) → folds method body
    - `object` (line 27) → folds through `fun value` body
    - `fun with_lambda` (line 32) → folds method body
    - `{(): String =>` (line 34) → folds through `"hello world"`
    - `fun with_recover` (line 39) → folds method body
    - `recover` (line 40) → folds through `consume s`
    - `class _D` (line 46) → folds through `fun dispose`

  type_alias.pony — multi-line type alias
    - `type TypeAlias is` (line 1) → folds through the closing `)`

  ifdef_expressions.pony — ifdef expression-level range
    - `class IfdefExpressions` (line 1) → folds entire class
    - `fun with_ifdef` (line 5) → folds method body
    - `ifdef debug then` (line 6) → folds through the else branch value

  entity_types.pony — all entity kinds and be behaviors
    - `primitive P` (line 1) → folds through `42`
    - `fun value(): U32 =>` (line 2) → folds through `42`
    - `struct S` (line 5) → folds through `x`
    - `new create() => None` — single-line, no fold indicator
    - `fun get(): U32 =>` (line 10) → folds through `x`
    - `actor A` (line 13) → folds entire actor body
    - `new create() => _n = 0` — single-line, no fold indicator
    - `be tick() =>` (line 18) → folds through `_n = _n + 1`
    - `fun count(): U32 =>` (line 21) → folds through `_n`
    - `trait T` (line 24) → folds through `required() * 2`
    - `fun required(): U32` — single-line (abstract), no fold indicator
    - `fun doubled(): U32 =>` (line 27) → folds through `required() * 2`
    - `interface EntityTypes` (line 30) → folds through `required() * 3`
    - `fun required(): U32` — single-line, no fold indicator
    - `fun tripled(): U32 =>` (line 33) → folds through `required() * 3`
"""
