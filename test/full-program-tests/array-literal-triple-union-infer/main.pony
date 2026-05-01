// Regression test for expr_array's element type accumulator leak
// (ponylang/ponyc#5205).
//
// `expr_array` in `src/libponyc/expr/array.c` accumulates a `type` across
// iterations by calling `type_union`, which may build a freshly-allocated
// TK_UNIONTYPE root. With three or more element types, the third iteration
// sees an orphaned fresh tree from the second iteration as its `prev_type`.
// `type_union` rebuilds a new fresh tree and discards the previous one —
// before the fix, that intermediate tree was leaked.

use @pony_exitcode[None](code: I32)

primitive _A
primitive _B
primitive _C

actor Main
  new create(env: Env) =>
    let arr = [_A; _B; _C]
    try
      match arr(0)?
      | _A => @pony_exitcode(1)
      | _B => None
      | _C => None
      end
    end
