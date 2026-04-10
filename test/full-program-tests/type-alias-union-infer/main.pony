// Regression test for find_infer_type's TK_TYPEALIASREF case (#5199),
// case (c): the recursive find_infer_type call returns a freshly-built
// tree from `type_union` (or `type_isect`).
//
// `find_infer_type` in `src/libponyc/expr/operator.c` dispatches on
// TK_TYPEALIASREF by unfolding the alias and recursing on the result.
// When the right-hand side of an inferring let binding is an aliased
// union type with no destructuring path, the recursive call hits the
// TK_UNIONTYPE branch which iterates the variants and assembles a new
// union via `type_union`. The recursive call returns a fresh tree that
// does not share storage with `unfolded`, so the fix must free both the
// fresh tree (via the `result != unfolded` inner free) and `unfolded`
// itself. This exercises the case (c) branch of the fix and confirms
// the inferred type still supports exhaustive matching against the
// union variants.

use @pony_exitcode[None](code: I32)

type _Either is (U64 | String)

actor Main
  new create(env: Env) =>
    let x: _Either = U64(42)
    let y = x
    match y
    | let n: U64 => if n == 42 then @pony_exitcode(1) end
    | let _: String => None
    end
