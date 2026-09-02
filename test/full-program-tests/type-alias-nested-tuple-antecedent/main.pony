// Regression test for a nested variant of the find_tuple_type leak.
//
// When a type alias unfolds to a union whose member is itself a type
// alias, find_tuple_type recurses through the union into the inner
// alias. The inner TK_TYPEALIASREF arm dups the TK_TUPLETYPE it finds
// and returns it. The outer TK_TYPEALIASREF arm then dups that result
// again but never frees the inner dup.
//
// See ponylang/ponyc#5958.

use @pony_exitcode[None](code: I32)

type _Inner is (U32, U64)
type _Outer is (_Inner | String)

actor Main
  new create(env: Env) =>
    let x: _Outer = (U32(10), U64(20))
    @pony_exitcode(1)
