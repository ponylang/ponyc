// Regression test for chained type aliases inside dynamically-traced tuples.
//
// An outer tuple wrapped in a union (which routes trace generation through
// trace_dynamic rather than the static trace_tuple path) with a child that
// is a chained alias to a tuple. The fix in ponylang/ponyc#5196 added a
// typealias_unfold inside trace_dynamic_tuple's TRACE_TUPLE branch but
// unfolded only one level, so a chained alias still arrived at the
// recursive trace_dynamic_tuple call as a TK_TYPEALIASREF and the
// iteration of alias-ref children as tuple elements tripped trace_type's
// default assertion. See ponylang/ponyc#5195.
//
// The fix made typealias_unfold transitive, so the swap value at the
// unfold site is guaranteed to be a concrete TK_TUPLETYPE even for
// chained aliases. This test exercises that path — the field type is
// `((String, _Middle) | None)` so trace dispatch goes through
// trace_dynamic, and `_Middle` is a chained alias to `_Pair`.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Main
  var _field: ((String, _Middle) | None) = None

  new create(env: Env) =>
    _field = ("hello", (U64(1), U64(2)))
    @pony_exitcode(1)
