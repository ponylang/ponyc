// Regression test for type alias elements inside dynamically-traced tuples.
//
// Two related scenarios in gentrace's trace_dynamic path:
//
// 1. An outer tuple has a child that is a type alias expanding directly to a
//    tuple. trace_dynamic_tuple's TRACE_TUPLE case used to iterate the alias
//    ref as if it were a tuple, tripping an assertion on the TK_ID child.
//
// 2. An outer tuple has a child that is a type alias expanding to a union
//    which in turn contains another alias that expands to a tuple.
//    trace_dynamic's TK_TYPEALIASREF case unfolded the alias into a detached
//    subtree; when that subtree recursed into trace_dynamic_tuple with
//    tuple != NULL, ast_swap asserted on the detached type's NULL parent.

type _Pair is (U64, U64)
type _Wrap is (_Pair | String)

actor Main
  var _direct: ((String, _Pair) | None) = None
  var _wrapped: ((String, _Wrap) | None) = None

  new create(env: Env) =>
    _direct = ("hello", (U64(1), U64(2)))
    _wrapped = ("world", (U64(3), U64(4)))
