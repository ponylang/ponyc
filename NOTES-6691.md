# Work notes: ponylang/ponyc#6691 (remove before opening the PR)

## The bug

In one GC trace epoch (one message send, receive, or local mark), the first visit to an object sets `obj->mark = gc->mark`, and every later visit to that object in the same epoch returns early. If the first visit is opaque (a tag), a later val or iso visit to the same object is skipped. That skipped visit would have recorded the mutability and traced the object's fields, so the fields go untraced. On a send this lets the sender's GC free the fields while the receiver still holds them (use-after-free). Example: `take(foo, foo)` where the parameters are `tag` then `val`, or an object with a tag field and a val field pointing at the same object.

## The fix on this branch (`src/libponyrt/gc/gc.c`)

- `object_t` gains `bool shallow` (`objectmap.h`/`.c`). It is true while every visit in the epoch stored in `mark` has been opaque.
- `begin_visit()` returns one of three results:
  - `VISIT_FIRST`: first visit this epoch. This path is unchanged from main.
  - `VISIT_DEEPEN`: first non-opaque visit after only opaque ones. It records mutability and recurses, with no rc change. If the opaque visit was untyped and left `obj->type` NULL, it fills the type in.
  - `VISIT_SKIP`: nothing to do.
- `send_remote_object`: on DEEPEN to immutable, when the object is not yet immutable, it adds `GC_INC_MORE`, acquires the object from its owner as immutable, and forces recursion.
- `recv_remote_object`: on DEEPEN, when rc == 1 and the visit is immutable and the type can't reference an actor, it adds `GC_IMMUT_HEAP_EQUIV` heap pressure. That pressure is skipped on the first visit when that visit was opaque.
- Release note: `.release-notes/orca-tag-before-val-or-iso.md`. The PR title must match its heading. Use the label `changelog - fixed` and put `Closes #6691` in the commit message.

## Tests

- `test/libponyrt/gc/gc.cc`, fixture `GcAliasOrder`: every tag/deep visit sequence of length 1 to 3, for all five ORCA functions (send/recv local, send/recv remote, mark remote), plus regression cases.
- Full-program tests (all fail on main, pass here, in both debug and release codegen):
  - `codegen-trace-object-tag-then-val`
  - `codegen-trace-object-tag-then-iso`
  - `codegen-trace-gc-tag-field-then-val-field`
  - `codegen-trace-gc-tag-field-then-iso-field`
  - `codegen-trace-gc-val-through-object-then-tag-field`

  Their helpers are in `codegen-trace/c-src/additional.cc`.
- Last verified state of this commit: `libponyrt.tests` passes, and the five full-program tests pass in both codegen modes.
- Not yet run on this exact commit:
  - the full `ctest -L ci-core` suite
  - pony-lint (no Pony source outside the tests changed)
  - the pre-PR code review

## Performance (callgrind instruction counts vs main)

Benchmark: 300 round trips of a 10,000-object message. Builds used `--cpu=x86-64` with an optimized runtime bitcode.

| Message | main | this branch | change |
|---|---|---|---|
| `Array[C] iso` | 4,330.9M | 4,348.4M | +0.40% |
| `Array[C] val` | 4,393.8M | 4,425.2M | +0.71% |
| `Array[C tag] val` | 4,261.9M | 4,279.6M | +0.42% |

This is roughly 2 to 4 extra instructions per object visit. Sean considers this cost significant and wants a cheaper design.

Alternatives tried, both measured worse (the branches are local only and not pushed):

1. Mark parity. `gc->mark` steps by 2. A tag-only visit stores `gc->mark + 1`, and the sweep in `actormap.c` accepts `mark` or `mark + 1`. Result: +0.89% (iso), +0.82% (val), both with a helper function and with the check written inline.
2. Deferring tags. Codegen emits new `pony_tracetag_known`/`pony_tracetag_unknown` calls for tags, and those push to a per-context buffer. `ponyint_gc_handlestack` visits that buffer after the stack drains, so every deep visit precedes every tag visit, and `gc.c` stays identical to main. Result: 0% on iso and val messages, but +11% on the tag message. With buffer reuse that would drop to about +4%. The cost per tag is the push, the loop, and an indirect call. Also, C code that calls `pony_traceknown(..., PONY_TRACE_OPAQUE)` directly would still have the bug.

## Pre-existing bugs found, separate from this fix (file as separate issues after the PR is open)

These are on main and are not caused by this PR:

- `acquire_object` reads the type from the object header. For a struct whose first field is embedded, that read gives the wrong type, and the owner's GC segfaults.
- Memory outside the Pony heap (an actor's embedded fields, FFI structs) is never deduplicated in a trace, so self-references hang the GC mark and the send.
- An interior-pointer self-cycle hangs the local GC mark.
- An immutable upgrade with `might_reference_actor == false` forces recursion on the sender, but the receiver doesn't recurse.

Earlier fixes and tests for these are on the local backup branch `orca-shared-address-wip`, which is not pushed.
