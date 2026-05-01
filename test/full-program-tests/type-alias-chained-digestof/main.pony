// Regression test for chained type aliases in gen_digestof_value.
//
// A chained alias (alias of alias of tuple) used as the declared type of a
// digestof operand reached LLVMStructTypeKind in gen_digestof_value, which
// unfolded the alias only one level before iterating tuple element types.
// The one-level unfold returned a still-TK_TYPEALIASREF, whose children are
// (TK_ID, typeargs, cap, eph) rather than tuple elements, tripping the
// `child == NULL` assertion after the iteration count reached the struct
// width. See ponylang/ponyc#5195.
//
// The fix made typealias_unfold transitive; the test exercises the
// digestof-of-local path specifically (a local variable so the actor has
// no fields, avoiding the companion static trace_tuple regression).

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Main
  new create(env: Env) =>
    let aliased: _Middle = (U64(1), U64(2))
    let plain: (U64, U64) = (U64(1), U64(2))
    if (digestof aliased) == (digestof plain) then
      @pony_exitcode(1)
    end
