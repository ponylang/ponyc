"""
Reproducer fixture for the systematic-testing reproducibility check (#5560).

Eight Sender actors each send 64 messages to a single Collector. The arrival
order is a pure function of the scheduler's interleaving; the Collector folds
(sender_id, seq) of every arrival into an order-sensitive hash and prints it as
ORDER_SIG. There is no randomness and no clock reads in the program itself, so
under systematic testing a seed-deterministic scheduler must print the same
ORDER_SIG for two runs at the same --ponysystematictestingseed, and different
seeds must be free to produce different ones.

Driven by .ci-scripts/systematic-testing/determinism_smoke.py, which builds
a systematic-testing ponyc, compiles this program with it, and asserts that
property under --ponynoscale. It is not part of the normal test suites.
"""
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @exit[None](status: I32)

actor Main
  new create(env: Env) =>
    let num_senders: USize = 8
    let per_sender: USize = 64
    let collector = Collector(num_senders * per_sender)
    var i: USize = 0
    while i < num_senders do
      Sender(collector, i, per_sender).start()
      i = i + 1
    end

actor Sender
  let _collector: Collector
  let _id: USize
  let _total: USize
  var _sent: USize = 0

  new create(collector: Collector, id: USize, total: USize) =>
    _collector = collector
    _id = id
    _total = total

  be start() =>
    tick()

  be tick() =>
    if _sent < _total then
      _collector.recv(_id, _sent)
      _sent = _sent + 1
      // self-send: a fresh behavior run, i.e. a scheduler interleaving point
      tick()
    end

actor Collector
  let _expected: USize
  var _received: USize = 0
  // FNV-1a-style rolling hash of the arrival order: an interleaving signature
  var _sig: U64 = 14695981039346656037

  new create(expected: USize) =>
    _expected = expected

  be recv(sender_id: USize, seq: USize) =>
    _mix(sender_id.u64())
    _mix(seq.u64())
    _received = _received + 1
    if _received == _expected then
      // Synchronous printf then forced exit. env.out.print is async and would be
      // lost on @exit. The forced exit also sidesteps the separate shutdown-hang
      // symptom under systematic testing. The signature is fully computed before
      // we exit, so the determinism result is unaffected.
      let line = "RECEIVED=" + _received.string()
        + " ORDER_SIG=" + _sig.string() + "\n"
      @printf("%s".cstring(), line.cstring())
      @exit(0)
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211
