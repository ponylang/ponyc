// Regression test for chained type aliases in tuple element access.
//
// `entity_access` in `src/libponyc/expr/postfix.c` unfolds the receiver
// type once and dispatches to `tuple_access` only if the unfolded head is
// TK_TUPLETYPE; otherwise it falls through to `member_access` which calls
// `lookup_base` which errors with "can't lookup by name on a tuple". For
// a chained alias to a tuple, the one-level unfold leaves a
// TK_TYPEALIASREF, the TK_TUPLETYPE check is false, and the access is
// rejected with the lookup error rather than dispatched to `tuple_access`.
// See ponylang/ponyc#5195.
//
// This test exercises that path via `t._1` / `t._2` on a parameter typed
// with a chained tuple alias.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

primitive Sum
  fun apply(t: _Middle): U64 =>
    t._1 + t._2

actor Main
  new create(env: Env) =>
    let p: _Middle = (U64(1), U64(2))
    if Sum(p) == 3 then
      @pony_exitcode(1)
    end
