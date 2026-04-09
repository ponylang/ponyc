// Regression test for chained type aliases in behavior message argument
// tracing.
//
// Sending a chained-alias-tuple argument to a behavior whose parameter
// is also typed with the same alias triggers `gen_send_message` in
// `gencall.c` to emit a GC-trace call for the argument. The trace
// codegen runs via `gentrace` -> `trace_type` (TRACE_TUPLE) ->
// `trace_tuple` at `gentrace.c:611`. The `if(ast_id(dst_type) ==
// TK_TUPLETYPE)` check is FALSE for an alias-typed parameter, so
// control falls into the else (boxed tuple) branch which iterates
// `ast_child(src_type)`. For a single-level or chained TK_TYPEALIASREF
// `src_type`, `ast_child` walks (TK_ID, typeargs, cap, eph); the
// recursive `gentrace` call on TK_ID hits `trace_type`'s default
// `pony_assert(0)` at `gentrace.c:383`.
//
// The transitive unfold in `typealias_unfold` (together with gentrace's
// top-level unfold of src/dst) resolves the chain to a concrete
// TK_TUPLETYPE before `trace_tuple` runs, so the iteration sees real
// tuple element children.
//
// This test exercises the message-send codegen path with both src and
// dst being alias types, distinct from the asymmetric pattern covered
// by type-alias-tuple-gentrace-needed and from the actor-field path
// covered by type-alias-chained-static-trace. See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Helper
  be receive(x: _Middle) =>
    @pony_exitcode(1)

actor Main
  new create(env: Env) =>
    let h = Helper
    h.receive((U64(1), U64(2)))
