// Regression test for chained type aliases as array element types.
//
// Building an `Array[_Middle]` where `_Middle` is a chained alias to a
// tuple triggers generation of the array's element trace function. The
// trace codegen calls `gentrace` on the element type, which dispatches
// via `trace_type` (`src/libponyc/codegen/gentrace.c:354`). For a chained
// alias, the one-level unfold path reaches `trace_type`'s default
// `pony_assert(0)` at line 383.
//
// This test exercises the array-element trace codegen path for chained
// aliases, distinct from the actor-field path covered by
// type-alias-chained-static-trace and the actor-message path covered by
// type-alias-chained-actor-message. See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Main
  new create(env: Env) =>
    let arr: Array[_Middle] = [(U64(1), U64(2)); (U64(3), U64(4))]
    try
      let first = arr(0)?
      let second = arr(1)?
      if ((first._1 + first._2) == 3) and ((second._1 + second._2) == 7) then
        @pony_exitcode(1)
      end
    else
      // Unreachable: arr(0) and arr(1) cannot fail on a 2-element array.
      // Use a distinct exit code so the test fails visibly if it ever does.
      @pony_exitcode(2)
    end
