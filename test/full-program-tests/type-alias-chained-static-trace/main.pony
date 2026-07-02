// Regression test for chained type aliases in the static trace_tuple path.
//
// An actor field whose type is a chained alias to a tuple reaches
// gentrace's TRACE_TUPLE branch when the actor's trace function is
// generated. The single-level typealias_unfold at gentrace.c entry left the
// field type as a TK_TYPEALIASREF, so the TK_TUPLETYPE check in
// trace_tuple was false and the else branch iterated the alias ref's
// children (TK_ID, typeargs, cap, eph) as if they were tuple elements,
// recursively calling gentrace on a TK_ID which tripped trace_type's
// default assertion. See ponylang/ponyc#5195.
//
// The fix made typealias_unfold transitive; this test exercises the
// static trace path via an actor field (no digestof, no union wrapping,
// which would route through gen_digestof_value or trace_dynamic
// respectively).

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Main
  var _data: _Middle = (U64(0), U64(0))

  new create(env: Env) =>
    _data = (U64(1), U64(2))
    @pony_exitcode(1)
