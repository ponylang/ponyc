// Positive-coverage test for chained type aliases over a class type with
// cap-restricted method dispatch.
//
// **Note on counterfactual sensitivity:** this test passes both with and
// without the transitive `typealias_unfold` fix, so it does not catch a
// regression in this PR's specific change on its own. The reason is that
// every code path that resolves chained class aliases for method
// dispatch (lookup.c, subtype.c) recurses on TK_TYPEALIASREF results
// from `typealias_unfold`, so the per-caller recursion handles a chain
// of length N regardless of whether the helper itself is single-level or
// transitive. Tuple-based tests in this PR exercise the actual crash
// sites; this test exists to document that chained class aliases work
// end-to-end (allocation through the chain, ref-cap method dispatch
// through the chain, box-cap read through the chain) and to guard
// against future regressions in that surface area. See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

class Counter
  var _count: U64 = 0

  fun ref bump() =>
    _count = _count + 1

  fun box current(): U64 =>
    _count

type _CounterAlias is Counter
type _ChainedCounter is _CounterAlias

actor Main
  new create(env: Env) =>
    // Allocate via the chained alias type. Resolution must walk the chain
    // (_ChainedCounter -> _CounterAlias -> Counter) and produce a value
    // whose cap permits the ref-restricted bump() call below.
    let c: _ChainedCounter = Counter

    // Calls a ref method. If cap propagation through the chain breaks,
    // this fails to compile (cap mismatch on the receiver).
    c.bump()
    c.bump()
    c.bump()

    // Calls a box method. Verifies the receiver is also box-compatible
    // (ref <: box) and the mutation observed by bump() actually happened.
    if c.current() == 3 then
      @pony_exitcode(1)
    end
