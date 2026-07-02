// Regression test for find_infer_type's TK_UNIONTYPE accumulator
// leak (ponylang/ponyc#5202), orphaned-`t` path.
//
// `find_infer_type` in `src/libponyc/expr/operator.c` iterates the
// children of a TK_UNIONTYPE and calls `type_union` with each result.
// When a child is a TK_TYPEALIASREF that unfolds to another union, the
// recursive call goes through the TK_TYPEALIASREF case introduced by
// #5203 and returns a freshly-dup'd orphan tree. The outer loop then
// calls `type_union(alias_prev, orphan_t)`, which builds a new fresh
// root and discards the orphan — before the fix, that orphan was
// leaked.
//
// Note: `typealias_unfold` is transitive only for chained top-level
// aliases (see #5201). Children of an unfolded union retain their
// TK_TYPEALIASREF form, so this nested structure survives into the
// TK_UNIONTYPE iteration as intended.
//
// The companion test type-alias-triple-union-infer covers the
// "orphaned prev_u" side where a multi-variant union rebuilds a fresh
// accumulator on each iteration.

use @pony_exitcode[None](code: I32)

primitive _A
primitive _B
primitive _C

type _Pair is (_B | _C)
type _Triple is (_A | _Pair)

actor Main
  new create(env: Env) =>
    let x: _Triple = _A
    let y = x
    match y
    | _A => @pony_exitcode(1)
    | _B => None
    | _C => None
    end
