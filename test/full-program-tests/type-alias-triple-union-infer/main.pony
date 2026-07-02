// Regression test for find_infer_type's TK_UNIONTYPE accumulator
// leak (ponylang/ponyc#5202), multi-iteration path.
//
// `find_infer_type` in `src/libponyc/expr/operator.c` accumulates a
// `u_type` across iterations by calling `type_union`, which may build a
// freshly-allocated TK_UNIONTYPE root. With three or more variants, the
// third iteration sees an orphaned fresh tree from the second iteration
// as its `prev_u`. `type_union` rebuilds a new fresh tree and discards
// the previous one — before the fix, that intermediate tree was leaked.
//
// This test exercises the "orphaned prev_u" side of the fix. The
// companion test type-alias-nested-union-infer covers the "orphaned t"
// side where `t` is a freshly dup'd tree returned from the
// TK_TYPEALIASREF case added in #5203.

use @pony_exitcode[None](code: I32)

primitive _A
primitive _B
primitive _C

type _Triple is (_A | _B | _C)

actor Main
  new create(env: Env) =>
    let x: _Triple = _A
    let y = x
    match y
    | _A => @pony_exitcode(1)
    | _B => None
    | _C => None
    end
