use "random"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @exit[None](status: I32)

primitive _Config
  fun generations(): USize => 50
  fun group(): USize => 6
  fun chains(): USize => 6
  fun ttl(): USize => 8
  // An arbitrary fixed seed for the per-Worker routing RNG. Its value does not
  // matter -- the route a chain takes depends on the interleaving, not just the
  // seed (see Worker.create) -- only that it is fixed so the program's own
  // randomness is not a source of run-to-run variation.
  fun routing_seed(): U64 => 8005939371141126624

primitive _Payload
  fun make(): String val =>
    // A freshly allocated, traced val payload -- gives ORCA something to trace
    // across the actor boundary, so the Workers fall idle and block between
    // bursts, which is what makes the cycle detector probe them.
    String.from_array(recover val Array[U8].init('p', 8) end)

actor Main
  new create(env: Env) =>
    // Exactly one chain terminates per injected chain, so the Collector expects
    // generations * chains completions before it folds the signature and exits.
    let collector = Collector(_Config.generations() * _Config.chains())
    Churner(collector, _Config.generations()).tick(0)

actor Churner
  """
  Spawns successive groups of Worker actors that form reference
  cycles, then drops them so only the cycle detector can reclaim
  them. Overlaps generations so reclamation races active pinging.
  """
  let _collector: Collector
  let _generations: USize

  new create(collector: Collector, generations: USize) =>
    _collector = collector
    _generations = generations

  be tick(gen: USize) =>
    """
    Spawn one generation of Workers, inject its chains, and
    schedule the next generation.
    """
    if gen >= _generations then
      return
    end

    // Build one group and give every member the whole group's array, so the
    // group is one strongly connected reference cycle. The array is held only
    // in this local; once the chains kicked off below drain, nothing outside
    // the group references any member and the group is garbage the cycle
    // detector must reclaim.
    let k = _Config.group()
    let group: Array[Worker] iso = recover Array[Worker](k) end
    var i: USize = 0
    while i < k do
      group.push(Worker(_collector, gen, i))
      i = i + 1
    end
    // Give every member the group before injecting any chain below. By Pony's
    // causal delivery each member's set_group is ordered ahead of any ping that
    // can reach it: the first ping of a chain comes from this Churner, which
    // sends set_group to all members first (same-sender FIFO), and a forwarded
    // ping can only come from a member that has therefore already run
    // set_group. So _group is always non-empty when ping() runs -- the `next`
    // index below and in Worker.ping cannot be out of range.
    let members: Array[Worker] val = consume group
    for w in members.values() do
      w.set_group(members)
    end

    // Inject this generation's chains from random starts. Routing is random, so
    // the path -- and each Worker's receive count when a chain terminates -- is
    // a pure function of the interleaving.
    let r = Rand(_Config.routing_seed(), gen.u64())
    var c: USize = 0
    while c < _Config.chains() do
      let start = r.int(k.u64()).usize()
      try
        members(start)?.ping(
          _Payload.make(),
          _Config.ttl(),
          (gen * _Config.chains()) + c)
      end
      c = c + 1
    end

    // Overlap generations so reclaiming earlier groups races the pinging of
    // later ones; that interleaving carries the detector's send ordering into
    // ORDER_SIG.
    tick(gen + 1)

actor Worker
  """
  A member of a cycle group. Forwards ping chains to random group
  members and reports the terminal hop to the Collector. Each group
  holds its own members array, forming a strongly connected
  reference cycle.
  """
  let _collector: Collector
  let _id: USize
  var _group: Array[Worker] val = recover val Array[Worker] end
  var _received: U64 = 0
  let _rand: Rand

  new create(collector: Collector, gen: USize, id: USize) =>
    _collector = collector
    _id = id
    // Per-Worker draw stream. The value stream is fixed by the seed; which
    // value lands on which ping depends on mailbox arrival order, so routing is
    // a function of the seed AND the interleaving.
    _rand = Rand(_Config.routing_seed(), ((gen * 31) + id).u64())

  be set_group(group: Array[Worker] val) =>
    _group = group

  be ping(payload: String val, hops: USize, chain: USize) =>
    _received = _received + 1
    if hops > 0 then
      // The incoming `payload` is intentionally dropped -- receiving the traced
      // `String val` is what creates the ORCA reference and the work, not its
      // contents. Forward a FRESH traced payload to a random group member.
      // `next` is always in [0, size), so the access cannot error.
      let next = _rand.int(_group.size().u64()).usize()
      try _group(next)?.ping(_Payload.make(), hops - 1, chain) end
    else
      _collector.completed(chain, _id, _received)
    end

actor Collector
  """
  Folds every chain completion's (chain, terminal_id,
  received_snapshot) into an order-sensitive FNV-1a hash and prints
  the resulting ORDER_SIG when all chains have completed.
  """
  let _expected: USize
  var _received: USize = 0
  // FNV-1a-style rolling hash of the arrival order: an interleaving signature.
  var _sig: U64 = 14695981039346656037

  new create(expected: USize) =>
    _expected = expected

  be completed(
    chain: USize,
    terminal_id: USize,
    received_snapshot: U64)
  =>
    """
    Record one chain completion and, when all have arrived, print
    the ORDER_SIG and exit.
    """
    _mix(chain.u64())
    _mix(terminal_id.u64())
    _mix(received_snapshot)
    _received = _received + 1
    if _received == _expected then
      // Synchronous printf then forced exit, as in the other fixtures:
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
