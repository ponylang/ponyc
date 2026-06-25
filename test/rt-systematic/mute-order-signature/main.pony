"""
Reproducer fixture for the systematic-testing reproducibility check (#5560),
covering the actor muting / unmuting scheduling path.

A ring of 48 Node actors each forwards a token to the next node a fixed number
of times (deterministic `(id + 1) % n` routing -- no randomness, no clock), then
reports the token's arrival to a single Collector. The Collector folds
(token_id, node_id) of every arrival into an order-sensitive hash printed as
ORDER_SIG, a pure function of the scheduler interleaving.

Unlike order-signature (whose senders only self-send and fan into the collector,
so no actor ever overloads another), the volume of foreign actor-to-actor sends
here drives backpressure: nodes overload, their senders get muted, and are later
rescheduled when the overloaded node drains. That unmute-rescheduling step is a
distinct part of the scheduler that order-signature does not exercise, so this
fixture guards a different slice of the "fixed seed replays one interleaving"
property.

The muting-driven reordering only manifests when scheduler-thread count and load
are in balance, so the node and token counts here are sized to overload actors
at the handful of physical cores a CI runner typically has (the runtime defaults
to one scheduler thread per physical core). See
.ci-scripts/systematic-testing/determinism_smoke.py, which builds a
systematic-testing ponyc, compiles this program with it, and asserts the
property under --ponynoscale. It is not part of the normal test suites.
"""
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @exit[None](status: I32)

actor Main
  new create(env: Env) =>
    let nodes: USize = 48
    let tokens: USize = 600
    let ttl: USize = 16
    let collector = Collector(tokens)
    let ring: Array[Node] iso = recover Array[Node](nodes) end
    var i: USize = 0
    while i < nodes do
      ring.push(Node(collector, i))
      i = i + 1
    end
    let ring': Array[Node] val = consume ring
    for node in ring'.values() do
      node.wire(ring')
    end
    // Wire every node before injecting tokens; causal messaging then delivers
    // each node's wire() ahead of any token() that can reach it, so _ring is
    // always set when token() runs.
    var t: USize = 0
    while t < tokens do
      try ring'(t % nodes)?.token(ttl, t) end
      t = t + 1
    end

actor Node
  let _collector: Collector
  let _id: USize
  var _ring: Array[Node] val = recover val Array[Node] end

  new create(collector: Collector, id: USize) =>
    _collector = collector
    _id = id

  be wire(ring: Array[Node] val) =>
    _ring = ring

  be token(ttl: USize, id: USize) =>
    if ttl > 0 then
      // Deterministic next hop: a foreign send that, in volume, overloads the
      // receiver and triggers the muting path. `next` is always in
      // [0, _ring.size()), so the indexed access cannot error.
      let next = (_id + 1) % _ring.size()
      try _ring(next)?.token(ttl - 1, id) end
    else
      _collector.recv(id, _id)
    end

actor Collector
  let _expected: USize
  var _received: USize = 0
  // FNV-1a-style rolling hash of the arrival order: an interleaving signature
  var _sig: U64 = 14695981039346656037

  new create(expected: USize) =>
    _expected = expected

  be recv(token_id: USize, node_id: USize) =>
    _mix(token_id.u64())
    _mix(node_id.u64())
    _received = _received + 1
    if _received == _expected then
      // Synchronous printf then forced exit, as in order-signature:
      // env.out.print is async and would be lost on @exit, and the forced exit
      // sidesteps the separate shutdown-hang symptom. The signature is fully
      // computed first, so the determinism result is unaffected.
      let line = "RECEIVED=" + _received.string()
        + " ORDER_SIG=" + _sig.string() + "\n"
      @printf("%s".cstring(), line.cstring())
      @exit(0)
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211
