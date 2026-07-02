// Regression test for gentrace_needed's TRACE_TUPLE branch with type
// aliases in src_type.
//
// `gentrace_needed` in `src/libponyc/codegen/gentrace.c:1043` iterates
// `ast_child(src_type)` in its TRACE_TUPLE case when `dst_type` is a
// concrete TK_TUPLETYPE. If `src_type` is a TK_TYPEALIASREF (single-level
// or chained), `ast_child` returns the alias ref's children
// (TK_ID, typeargs, cap, eph) rather than tuple elements. The recursive
// `gentrace_needed(c, src_child, dst_child)` then calls `trace_type` on a
// TK_ID, tripping the default `pony_assert(0)` at line 383.
//
// This is a #5145 regression that the prior per-site fixes (#5193, #5196)
// did not catch. Both single-level and chained aliases trigger it. The
// fix unfolds src_type and dst_type before iterating, matching the
// pattern used by the TRACE_MACHINE_WORD case in the same function. See
// ponylang/ponyc#5195.
//
// The asymmetric type pattern (alias src, concrete dst) is what
// distinguishes this site from the actor-message test, where both src
// and dst are alias types and the iteration branch is never entered.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Helper
  be receive(x: (U64, U64)) =>
    if (x._1 + x._2) == 3 then
      @pony_exitcode(1)
    end

actor Main
  new create(env: Env) =>
    let single: _Pair = (U64(1), U64(2))
    let chained: _Middle = (U64(1), U64(2))
    let h = Helper
    // Send via chained alias — the argument is _Middle, the parameter is
    // (U64, U64). Asymmetry triggers gentrace_needed's iteration branch.
    h.receive(chained)
    // Also exercise the single-level alias path. (Send order matters
    // less than that both compile cleanly.)
    h.receive(single)
