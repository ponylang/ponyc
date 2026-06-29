"""
Generative runtime stress harness.

A swarm-driven Pony program that exercises the runtime's message passing, ORCA
tracing, the cycle detector's collection path (via the `cyclic` workload), and the
explicit backpressure / muting path (via the `backpressure` workload). One run
draws one of three closed, count-driven workloads (`--workload`):

* `mesh` (M0): a closed mesh of `Pinger` actors. A fixed number of chains are
  injected, each carrying a hop counter (TTL); a `Pinger` receiving a ping with
  hops > 0 forwards exactly one ping (hops - 1) to a random neighbor, and a ping
  with hops == 0 terminates and signals the coordinator. Because each ping
  produces exactly one successor message the run terminates deterministically
  once every chain has terminated, and

      sent == received == chains * (ttl + 1)

  is an exact, checkable invariant the coordinator verifies by polling every
  `Pinger`'s send/receive tally.

* `cyclic` (M1): successive generations of `CyclicWorker` groups. Each worker in
  a group holds the whole group's `val` array, so a group is one strongly
  connected reference cycle that reference counting alone can never reclaim. The
  group's external reference is dropped after its chains are injected, so it
  becomes garbage only the cycle detector can collect. This is the proven shape of
  `test/rt-systematic/cycle-collection-order-signature`, parameterized and wired
  to this engine's CLI/config/ORDER_SIG conventions (an independent program, not a
  shared module -- the two can diverge without breaking each other). Because the
  workers become garbage they cannot be polled for tallies, so conservation here
  is completion-count only: exactly `generations * chains` terminal completions
  must arrive. A lost forward mid-chain leaves a chain that never completes -- the
  count never reaches its target and the run is caught by the orchestrator's
  liveness watchdog as a timeout, not as an in-engine conservation failure. A
  duplicate or late completion drives the count past its target and trips
  `_Fatal`. This is strictly weaker than the mesh's independent send/receive
  tally; it is the strongest oracle available once the actors are garbage.

* `backpressure` (M1): a star of `Producer` actors flooding one `Consumer`. Each
  producer sends `messages` work messages then a `finished` report; the consumer
  explicitly participates in runtime backpressure via the stdlib `backpressure`
  package -- after every `apply_every` messages it calls `Backpressure.apply`
  (marking itself UNDER_PRESSURE, so its producers are muted) and self-sends a
  `lift` that calls `Backpressure.release`. The self-send is never muted, so apply
  is always paired with release and the closed flood cannot deadlock. This drives
  the explicit UNDER_PRESSURE muting path that the `mesh` never reaches -- the mesh
  drives only the runtime's *automatic* OVERLOAD muting. Conservation is the mesh's
  strength, not the cyclic's weakness: completion triggers on
  `finished_count == producers` (independent of the received count), then asserts
  `received == sent == producers * messages` from the producers' own send tallies,
  so a lost work is caught as a conservation failure here, not merely as a timeout.

The workloads are closed, unlike an open cascade stopped by a wall-clock timer:
a fixed amount of work is injected and the run terminates when it drains. The
closed shape is a deliberate constraint, not the end goal -- a continuous, open
workload is the better stress model, but closed is what's feasible under
systematic testing today (it needs no timer; time is not virtualized there). When
open becomes feasible, revisit this.

On success the engine prints its result line with a synchronous `@printf` and then
*returns*, letting the program reach natural quiescence rather than forcing an
exit. Two reasons: the `cyclic` workload's garbage is only collected as the
program quiesces (a forced exit kills the process before the detector sweeps), and
letting the runtime shut down cleanly turns shutdown itself into something the
harness tests -- the systematic park-site hang fixed by #5576/#5585 surfaces here
as a watchdog timeout if it ever regresses (see .known-couplings/
systematic-testing-park-sites.md). Only a conservation *failure* forces `@exit(1)`:
once a bug is detected, fail fast rather than risk quiescing a broken run.

All randomness is seeded only from `--seed` (never the clock). The routing graph
is not a function of `--seed` alone, though: which draw lands on which ping depends
on the order an actor processes its mailbox, so the routing -- and the run --
reproduce only when the interleaving does: a fixed `--ponysystematictestingseed`
under a systematic build. (#5566/#5570 made that replay independent of memory
layout, so disabling ASLR is no longer needed.) Each workload's driver/collector
folds its terminal arrivals (chain completions, or producer `finished` reports)
into an FNV-1a `ORDER_SIG` -- a pure function of the scheduler interleaving -- as
the observable for the determinism oracle.

This program holds no `@runtime_override_defaults`: every runtime knob (scaling,
cycle detector, GC, thread count, the systematic seed) is a `--pony*` flag set by
the orchestrator, and every workload parameter is a `--flag value` arg parsed
here. Two orchestrators in this directory drive it: `orchestrate_systematic.py`
(serialized, reproducible -- draws only the `mesh` workload today) and
`orchestrate_normal.py` (a normal, real-parallel runtime -- draws all three
workloads; the conservation invariant holds there too, but `ORDER_SIG` does not
reproduce, so that mode runs each seed once).
"""
use "backpressure"
use "cli"
use "random"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stderr[Pointer[U8]]()
use @exit[None](status: I32)

type Payload is (String val | U64)
  """
  A message's traced cargo: a heap-allocated `String val` (gives ORCA something to
  trace across the actor boundary) or a `U64` primitive (a no-GC control). The
  kind is fixed for a whole run by `--payload`; `--payload-mode` then decides
  whether one payload object is reused (forwarded the whole chain, or re-sent by a
  producer) or a fresh one is allocated at each send. Shared by all workloads.
  """

primitive _Payloads
  """
  Builds a message payload of the configured kind. `--payload-mode forward` reuses
  one object across a sender's sends (exercising ORCA's shared-`val` refcounting);
  `fresh` calls this at every send (exercising allocation + tracing + collection
  churn). Used by all three workloads.
  """
  fun make(config: _Config): Payload =>
    if config.payload_string then
      let s: String val =
        String.from_array(recover val Array[U8].init('p', config.payload_size) end)
      s
    else
      U64(42)
    end

actor Main
  new create(env: Env) =>
    """
    Parse the workload args with the `cli` package and dispatch on the workload
    kind. `--help` prints usage and exits 0; a syntax error (unknown flag, missing
    value, bad number) or a failed value check exits non-zero without starting the
    workload.
    """
    let spec =
      try
        _Cli.spec()?
      else
        env.err.print("internal error: malformed command spec")
        env.exitcode(1)
        return
      end
    match \exhaustive\ CommandParser(spec).parse(env.args)
    | let cmd: Command =>
      match _Cli.config(cmd)
      | let config: _Config =>
        match config.workload
        | let mesh: _Mesh => Coordinator(config, mesh)
        | let cyclic: _Cyclic => Churner(config, cyclic)
        | let bp: _Backpressure =>
          // Only this workload needs the backpressure auth token, built from the
          // root authority here and threaded into the Consumer.
          Consumer(config, bp, ApplyReleaseBackpressureAuth(env.root))
        end
      | let err: String =>
        env.err.print(err)
        env.exitcode(1)
      end
    | let help: CommandHelp =>
      help.print_help(env.out)
    | let se: SyntaxError =>
      env.err.print(se.string())
      env.exitcode(1)
    end

actor Coordinator
  """
  The `mesh` workload driver. Builds the mesh, injects the chains, counts
  completions to detect quiescence, collects per-`Pinger` send/receive tallies,
  and evaluates the conservation oracle. On completion it prints `ORDER_SIG` plus
  the tallies; on a conservation failure it forces a non-zero exit, otherwise it
  returns and lets the program quiesce.
  """
  let _config: _Config
  let _mesh: _Mesh
  let _pingers: Array[Pinger] val
  var _completions: USize = 0
  var _reports_outstanding: USize = 0
  var _total_sent: U64 = 0
  var _total_received: U64 = 0
  var _sig: U64 = 14695981039346656037 // FNV-1a 64-bit offset basis

  new create(config: _Config, mesh: _Mesh) =>
    _config = config
    _mesh = mesh

    // Create the mesh. Pingers are built outside the `recover` block (so we can
    // pass `this`) and pushed into the iso array via automatic receiver
    // recovery -- `push`'s argument, a `Pinger tag`, is sendable.
    let ps: Array[Pinger] iso = recover Array[Pinger](mesh.pingers) end
    var id: USize = 0
    while id < mesh.pingers do
      ps.push(Pinger(this, id, config))
      id = id + 1
    end
    let pingers: Array[Pinger] val = consume ps
    _pingers = pingers

    // Wire neighbors, THEN inject. The ordering is load-bearing -- see the
    // comment on `Pinger.set_neighbors`.
    for p in pingers.values() do
      p.set_neighbors(pingers)
    end

    // Each injected ping is one coordinator send (added back in `_finish`).
    let r = Rand(config.seed)
    var chain: USize = 0
    while chain < mesh.chains do
      let start = r.int(pingers.size().u64()).usize()
      try
        pingers(start)?.ping(_Payloads.make(config), mesh.ttl, chain)
      else
        _Unreachable("injection start index out of range")
      end
      chain = chain + 1
    end

  be completed(chain: USize, terminal_id: USize, received_snapshot: U64) =>
    """
    A chain reached its terminal (hops == 0) ping. Fold the arrival into the
    order signature; once every chain has terminated the system is quiesced (see
    `Pinger.set_neighbors` and the module docstring), so ask every `Pinger` for
    its final counts.
    """
    _mix(chain.u64())
    _mix(terminal_id.u64())
    _mix(received_snapshot)
    _completions = _completions + 1
    if _completions == _mesh.chains then
      _reports_outstanding = _pingers.size()
      for p in _pingers.values() do
        p.report()
      end
    end

  be report_counts(sent: U64, received: U64) =>
    """
    Accumulate one `Pinger`'s tallies. When all have reported, evaluate the
    conservation oracle.
    """
    _total_sent = _total_sent + sent
    _total_received = _total_received + received
    _reports_outstanding = _reports_outstanding - 1
    if _reports_outstanding == 0 then
      _finish()
    end

  fun ref _finish() =>
    // The coordinator itself performed `chains` initial sends (the injected
    // pings); every `Pinger` forward is already in `_total_sent`.
    let total_sent = _total_sent + _mesh.chains.u64()
    let expected = _mesh.chains.u64() * (_mesh.ttl.u64() + 1)
    let conserved =
      (total_sent == _total_received) and (_total_received == expected)

    // Synchronous print so the result survives even though we do not force an
    // exit: env.out.print is async and could be lost if a later exit raced it.
    let line = "RECEIVED=" + _total_received.string()
      + " SENT=" + total_sent.string()
      + " EXPECTED=" + expected.string()
      + " ORDER_SIG=" + _sig.string() + "\n"
    @printf("%s".cstring(), line.cstring())

    // On success: return and let the program reach natural quiescence (no forced
    // exit). The mesh + coordinator form one reference cycle that, once this
    // behavior returns, is unreachable; with the cycle detector on it is collected
    // as the program shuts down -- bonus detector exercise -- and either way the
    // runtime quiesces cleanly (the shutdown hang a forced exit once dodged is
    // fixed by #5576/#5585). Only a conservation FAILURE forces a non-zero exit:
    // fail fast once a bug is detected rather than risk quiescing a broken run.
    if not conserved then
      let diag = "CONSERVATION FAILURE: received=" + _total_received.string()
        + " sent=" + total_sent.string()
        + " expected=" + expected.string() + "\n"
      @fprintf(@pony_os_stderr(), "%s".cstring(), diag.cstring())
      @exit(I32(1))
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211 // FNV-1a 64-bit prime

actor Pinger
  """
  A mesh node. Forwards exactly one successor message per received ping and
  tracks how many pings it has sent and received.
  """
  let _coord: Coordinator
  let _id: USize
  let _config: _Config
  var _neighbors: Array[Pinger] val
  var _received: U64 = 0
  var _sent: U64 = 0
  var _reported: Bool = false
  let _rand: Rand

  new create(coord: Coordinator, id: USize, config: _Config) =>
    _coord = coord
    _id = id
    _config = config
    _neighbors = recover val Array[Pinger] end
    // Per-Pinger deterministic draw stream, seeded only from `--seed` + id,
    // never the clock. The stream of values is fixed by the seed; which value
    // lands on which ping depends on mailbox arrival order, so the resulting
    // routing is a function of the seed AND the interleaving (see module docs).
    _rand = Rand(config.seed, id.u64())

  be set_neighbors(neighbors: Array[Pinger] val) =>
    """
    Install the full mesh. The coordinator sends this to every `Pinger` BEFORE
    it injects any chain (program order in the coordinator's constructor), so by
    Pony's causal message delivery every `Pinger` has its neighbors before any
    ping -- the coordinator's injected ping or any forward descended from one --
    can converge on it. Injecting before wiring would let a `Pinger` forward
    before it knows its neighbors.
    """
    _neighbors = neighbors

  be ping(payload: Payload, hops: USize, chain: USize) =>
    """
    Produce exactly one successor message per received ping: a forward when
    hops > 0, otherwise one completion signal. This one-in/one-out invariant is
    what makes the coordinator's completion count a sound quiescence barrier
    (module docstring) -- do not make this fan out.
    """
    if _reported then
      // Quiescence is proven before `report()` is sent, so no ping should
      // arrive afterwards. One that does is a leaked or duplicated message.
      _Fatal("ping after report (leaked/late message)")
    end
    _received = _received + 1
    if hops > 0 then
      _sent = _sent + 1
      // In `fresh` mode allocate a new payload each hop (allocation +
      // collection churn); in `forward` mode re-send the received object
      // (shared-`val` refcounting). Either way exactly one message goes out.
      let outgoing: Payload =
        if _config.payload_fresh then _Payloads.make(_config) else payload end
      let next = _rand.int(_neighbors.size().u64()).usize()
      try
        _neighbors(next)?.ping(outgoing, hops - 1, chain)
      else
        _Unreachable("neighbor index out of range")
      end
    else
      _coord.completed(chain, _id, _received)
    end

  be report() =>
    _reported = true
    _coord.report_counts(_sent, _received)

actor Churner
  """
  The `cyclic` workload driver. Spawns `generations` successive groups of
  `CyclicWorker` actors; each worker holds the whole group's array, so a group is
  one strongly connected reference cycle. After a generation's chains are injected
  the group's only external reference (the local below) is dropped, so the group
  becomes garbage only the cycle detector can reclaim. Generations overlap --
  reclaiming an earlier group races the pinging of a later one -- so the detector
  works under sustained load. Adapted from
  `test/rt-systematic/cycle-collection-order-signature`.
  """
  let _config: _Config
  let _generations: USize
  let _group: USize
  let _chains: USize
  let _ttl: USize
  let _collector: CyclicCollector

  new create(config: _Config, cyclic: _Cyclic) =>
    _config = config
    _generations = cyclic.generations
    _group = cyclic.group
    _chains = cyclic.chains
    _ttl = cyclic.ttl
    // Exactly one chain terminates per injected chain, so the collector expects
    // generations * chains completions before it folds the signature.
    _collector = CyclicCollector(cyclic.generations * cyclic.chains)
    tick(0)

  be tick(gen: USize) =>
    """
    Build generation `gen`'s group, wire every member to the whole group (one
    strongly connected cycle), inject its chains, then self-send the next
    generation. The group's only external reference is this behavior's local, so
    once the chains drain the group is unreachable garbage the cycle detector must
    reclaim. Self-sending the next generation before this one drains is what makes
    the generations overlap.
    """
    if gen >= _generations then
      return
    end

    // Build one group and give every member the whole group's array, so the
    // group is one strongly connected reference cycle. The array is held only in
    // this local; once the chains kicked off below drain, nothing outside the
    // group references any member and the group is garbage the cycle detector
    // must reclaim.
    let k = _group
    let group: Array[CyclicWorker] iso = recover Array[CyclicWorker](k) end
    var i: USize = 0
    while i < k do
      group.push(CyclicWorker(_collector, _config, gen, i))
      i = i + 1
    end
    // Give every member the group before injecting any chain, so by causal
    // delivery each member's set_group is ordered ahead of any ping that can
    // reach it (same reasoning as Pinger.set_neighbors): the first ping comes
    // from this Churner, which sends set_group to all members first (same-sender
    // FIFO), and a forwarded ping can only come from a member that has therefore
    // already run set_group. So `_group` is always non-empty when ping() runs.
    let members: Array[CyclicWorker] val = consume group
    for w in members.values() do
      w.set_group(members)
    end

    // Inject this generation's chains from random starts. Routing is random, so
    // the path -- and each worker's receive count when a chain terminates -- is a
    // pure function of the interleaving.
    let r = Rand(_config.seed, gen.u64())
    var c: USize = 0
    while c < _chains do
      let start = r.int(k.u64()).usize()
      try
        members(start)?.ping(_Payloads.make(_config), _ttl,
          (gen * _chains) + c)
      else
        _Unreachable("cyclic injection start index out of range")
      end
      c = c + 1
    end

    // Overlap generations so reclaiming earlier groups races the pinging of
    // later ones; that interleaving carries the detector's send ordering into
    // ORDER_SIG.
    tick(gen + 1)

actor CyclicWorker
  """
  A member of one strongly connected group. Forwards exactly one successor
  message per received ping (the same one-in/one-out invariant as `Pinger`), then
  the group becomes unreachable and the worker is collected by the cycle detector.
  It is never polled for tallies (it is garbage by quiescence), so it keeps no
  report path.
  """
  let _collector: CyclicCollector
  let _config: _Config
  let _id: USize
  var _group: Array[CyclicWorker] val
  var _received: U64 = 0
  let _rand: Rand

  new create(collector: CyclicCollector, config: _Config, gen: USize, id: USize) =>
    _collector = collector
    _config = config
    _id = id
    _group = recover val Array[CyclicWorker] end
    // Per-worker draw stream. The value stream is fixed by the seed; which value
    // lands on which ping depends on mailbox arrival order, so routing is a
    // function of the seed AND the interleaving.
    _rand = Rand(config.seed, ((gen * 31) + id).u64())

  be set_group(group: Array[CyclicWorker] val) =>
    """
    Install the group array (which makes this worker part of the cycle). The
    Churner sends this to every member before injecting any chain, so by causal
    delivery `_group` is always non-empty when `ping` runs (same reasoning as
    `Pinger.set_neighbors`).
    """
    _group = group

  be ping(payload: Payload, hops: USize, chain: USize) =>
    """
    Produce exactly one successor message per received ping: a forward when
    hops > 0, otherwise one completion signal. The same one-in/one-out invariant
    as `Pinger.ping` -- it is what makes exactly `generations * chains` completions
    the sound quiescence/conservation barrier (module docstring).
    """
    _received = _received + 1
    if hops > 0 then
      let outgoing: Payload =
        if _config.payload_fresh then _Payloads.make(_config) else payload end
      let next = _rand.int(_group.size().u64()).usize()
      try
        _group(next)?.ping(outgoing, hops - 1, chain)
      else
        _Unreachable("cyclic neighbor index out of range")
      end
    else
      _collector.completed(chain, _id, _received)
    end

actor CyclicCollector
  """
  Counts the `cyclic` workload's chain completions, folds each into `ORDER_SIG`,
  and is the conservation oracle: exactly `expected` (generations * chains)
  completions must arrive. On the expected-th it prints the result and returns,
  letting the program quiesce. A completion beyond the expected total is a
  duplicate or late message -- a real fault -- so it trips `_Fatal`. (A *lost*
  message instead leaves the count short forever; that surfaces as the
  orchestrator's liveness timeout, not here.) The collector stays reachable for a
  genuine duplicate because the worker that produced it still references the
  collector.
  """
  let _expected: USize
  var _received: USize = 0
  var _sig: U64 = 14695981039346656037 // FNV-1a 64-bit offset basis

  new create(expected: USize) =>
    _expected = expected

  be completed(chain: USize, terminal_id: USize, received_snapshot: U64) =>
    """
    Fold one chain's terminal arrival into `ORDER_SIG` and count it. On the
    expected-th completion print the result (then quiesce); a completion beyond the
    expected total is a duplicate or late message, so trip `_Fatal`.
    """
    _mix(chain.u64())
    _mix(terminal_id.u64())
    _mix(received_snapshot)
    _received = _received + 1
    if _received == _expected then
      // Synchronous print so the result survives natural quiescence (no SENT --
      // the garbage workers cannot be polled for a send tally; see module docs).
      let line = "RECEIVED=" + _received.string()
        + " EXPECTED=" + _expected.string()
        + " ORDER_SIG=" + _sig.string() + "\n"
      @printf("%s".cstring(), line.cstring())
    elseif _received > _expected then
      _Fatal("late/duplicate completion (received > expected)")
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211 // FNV-1a 64-bit prime

actor Producer
  """
  A `backpressure`-workload producer. Sends `messages` work messages to the single
  consumer, then one `finished` report carrying its id and send tally, and stops.
  Each foreign `work` send is a muting point: while the consumer is UNDER_PRESSURE
  the producer is muted, so its self-sent `produce` is deferred until the consumer
  releases -- that deferral IS the backpressure. Many producers flood one consumer
  (a star), so every muted producer concentrates in that one receiver's muteset.
  """
  let _consumer: Consumer
  let _config: _Config
  let _id: USize
  var _remaining: USize
  var _sent: U64 = 0
  let _payload: Payload

  new create(consumer: Consumer, config: _Config, id: USize, messages: USize) =>
    _consumer = consumer
    _config = config
    _id = id
    _remaining = messages
    // `forward` mode reuses this one payload object for every send (a muted
    // producer then holds a shared `val` ref -- ORCA under backpressure); `fresh`
    // mode allocates a new one at each send (allocation churn). Built once here so
    // forward mode has an object to reuse.
    _payload = _Payloads.make(config)
    produce()

  be produce() =>
    """
    Send exactly one work message, then self-send to continue; after `messages`
    have been sent, report `finished` and stop. The self-send is deferred while
    this producer is muted, so production pauses under backpressure and resumes on
    release -- without any explicit rate logic here.
    """
    if _remaining == 0 then
      _consumer.finished(_id, _sent)
      return
    end
    let outgoing: Payload =
      if _config.payload_fresh then _Payloads.make(_config) else _payload end
    _consumer.work(outgoing)
    _sent = _sent + 1
    _remaining = _remaining - 1
    produce()

actor Consumer
  """
  The `backpressure` workload's driver, sink, and conservation oracle in one actor
  (mirroring how the mesh's `Coordinator` both drives and evaluates). It spawns the
  producers, counts received work, participates in runtime backpressure, and on
  completion checks conservation.

  Backpressure: after every `apply_every` received work messages it calls
  `Backpressure.apply` (marking itself UNDER_PRESSURE, so producers sending to it
  are muted) and self-sends `lift`; processing the `lift` calls
  `Backpressure.release` (unmuting them). The `lift` self-send is immune to muting
  (a self-send never mutes -- the `ctx->current != to` rule in actor.c
  `maybe_mark_should_mute`), so every apply is followed by a release: the closed
  flood cannot deadlock. This drives the explicit UNDER_PRESSURE muting path that
  the mesh's automatic OVERLOAD muting never reaches.

  Conservation: each producer sends `finished(id, sent)` after its last work; by
  same-sender FIFO that report follows all of its work, so when all `producers`
  have reported, all work has been received. The oracle then asserts
  received == total_sent == expected (producers * messages): a lost work shows as
  received < total_sent (a conservation failure caught here, not just a timeout), a
  duplicate trips `_Fatal`. On success it returns and the program quiesces.
  """
  let _auth: ApplyReleaseBackpressureAuth
  let _producers: USize
  let _apply_every: USize
  let _expected: U64
  var _received: U64 = 0
  var _since_apply: USize = 0
  var _pressured: Bool = false
  var _total_sent: U64 = 0
  var _finished: USize = 0
  var _done: Bool = false
  var _sig: U64 = 14695981039346656037 // FNV-1a 64-bit offset basis

  new create(config: _Config, bp: _Backpressure,
    auth: ApplyReleaseBackpressureAuth)
  =>
    _auth = auth
    _producers = bp.producers
    _apply_every = bp.apply_every
    _expected = bp.producers.u64() * bp.messages.u64()

    // Spawn the producers pointing at this consumer (the Coordinator pattern: the
    // driver creates the workers with `this`). Each floods `messages` work
    // messages and then reports `finished`.
    var id: USize = 0
    while id < bp.producers do
      Producer(this, config, id, bp.messages)
      id = id + 1
    end

  be work(payload: Payload) =>
    """
    Count one received work message and run the backpressure hysteresis. The
    payload is traced by ORCA across the send/receive crossing and then dropped
    (the consumer doesn't retain it). A work after completion is a leaked/late
    message -- a real fault -- so it trips `_Fatal`.
    """
    if _done then
      _Fatal("work after done (leaked/late message)")
    end
    _received = _received + 1
    if not _pressured then
      _since_apply = _since_apply + 1
      if _since_apply >= _apply_every then
        // DEADLOCK-FREEDOM INVARIANT: this Consumer must never do a foreign send.
        // While UNDER_PRESSURE, a foreign send could mute this Consumer itself, and
        // the explicit Backpressure.release is the SOLE guaranteed unmute for a
        // pressure-mute (the runtime's automatic overload-clear is subordinate --
        // gated by !UNDER_PRESSURE), so a muted Consumer could never release and the
        // flood would hang. Its only send is the `lift` self-send below, which is
        // immune to muting. See .known-couplings/backpressure-workload-muting.md.
        Backpressure.apply(_auth)
        _pressured = true
        lift()
      end
    end

  be lift() =>
    """
    Release backpressure. Reached only via the self-send queued when pressure was
    applied; a self-send is never muted, so this always runs and always releases --
    the apply/release pairing that makes the closed flood deadlock-free.
    """
    if _pressured then
      Backpressure.release(_auth)
      _pressured = false
      _since_apply = 0
    end

  be finished(producer_id: USize, sent: U64) =>
    """
    Fold one producer's terminal report into `ORDER_SIG` (one fold per producer,
    matching the mesh/cyclic fold-at-completion convention) and accumulate its send
    tally. When all producers have reported, evaluate conservation. A duplicate or
    late `finished` arrives after `_finish` has set `_done` (the report that reaches
    `_producers` triggers `_finish` synchronously), so the `_done` guard catches it.
    A *lost* `finished` instead leaves `_finished` short forever -- caught by the
    orchestrator's liveness timeout, not here (the symmetric case to a lost `work`,
    which conservation catches; mirrors the cyclic workload's lost-completion note).
    """
    if _done then
      _Fatal("finished after done (duplicate/late report)")
    end
    _mix(producer_id.u64())
    _mix(sent)
    _total_sent = _total_sent + sent
    _finished = _finished + 1
    if _finished == _producers then
      _finish()
    end

  fun ref _finish() =>
    _done = true
    // All producers have reported, so by same-sender FIFO all work has been
    // received. SENT is the independent per-producer tally (as strong as the
    // mesh's); a lost work shows here as received < total_sent.
    let conserved = (_received == _total_sent) and (_total_sent == _expected)

    // Release if still under pressure so the program quiesces cleanly rather than
    // depending on a pending `lift` running first. Idempotent: `lift` guards on
    // `_pressured`, so a queued `lift` then no-ops.
    if _pressured then
      Backpressure.release(_auth)
      _pressured = false
    end

    // Synchronous print so the result survives natural quiescence (env.out.print
    // is async and could be lost if a later exit raced it).
    let line = "RECEIVED=" + _received.string()
      + " SENT=" + _total_sent.string()
      + " EXPECTED=" + _expected.string()
      + " ORDER_SIG=" + _sig.string() + "\n"
    @printf("%s".cstring(), line.cstring())

    // On success: return and let the program reach natural quiescence (no forced
    // exit), same as the mesh/cyclic. Only a conservation FAILURE forces a
    // non-zero exit: fail fast once a bug is detected.
    if not conserved then
      let diag = "CONSERVATION FAILURE: received=" + _received.string()
        + " sent=" + _total_sent.string()
        + " expected=" + _expected.string() + "\n"
      @fprintf(@pony_os_stderr(), "%s".cstring(), diag.cstring())
      @exit(I32(1))
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211 // FNV-1a 64-bit prime

class val _Mesh
  """
  The `mesh` workload's shape. Built only by `_Cli.config` (after validation), so
  `pingers >= 1` and `chains >= 1` can be trusted. Carries the chain count and hop
  count itself (rather than sharing them through `_Config`) so a `backpressure`
  config -- which has no chains/ttl -- cannot carry those fields at all; an illegal
  cross-workload field mix is unrepresentable.
  """
  let pingers: USize
  let chains: USize
  let ttl: USize

  new val create(pingers': USize, chains': USize, ttl': USize) =>
    pingers = pingers'
    chains = chains'
    ttl = ttl'

class val _Cyclic
  """
  The `cyclic` workload's shape. Built only by `_Cli.config` (after validation),
  so `generations >= 1`, `group >= 2` (a group needs at least two members to be a
  non-trivial cycle), and `chains >= 1` can be trusted. Carries chains/ttl itself
  for the same reason as `_Mesh`.
  """
  let generations: USize
  let group: USize
  let chains: USize
  let ttl: USize

  new val create(generations': USize, group': USize, chains': USize,
    ttl': USize)
  =>
    generations = generations'
    group = group'
    chains = chains'
    ttl = ttl'

class val _Backpressure
  """
  The `backpressure` workload's shape. Built only by `_Cli.config` (after
  validation), so `producers >= 1`, `messages >= 1`, and `apply_every >= 1` can be
  trusted. Has no chains/ttl: a flood is not a chain of hops, so those fields are
  structurally absent here (the reason chains/ttl live on `_Mesh`/`_Cyclic` rather
  than on the shared `_Config`).
  """
  let producers: USize
  let messages: USize
  let apply_every: USize

  new val create(producers': USize, messages': USize, apply_every': USize) =>
    producers = producers'
    messages = messages'
    apply_every = apply_every'

class val _Config
  """
  A validated workload configuration: the fields shared by all workloads (seed and
  payload kind/size/mode) plus the workload-specific shape (`_Mesh`, `_Cyclic`, or
  `_Backpressure`). The chain/hop counts are NOT here -- only the two chain-based
  workloads have them, so they live on those variants. Built only by `_Cli.config`,
  so the rest of the program can trust its invariants (payload_size >= 1 when
  payload_string, plus the per-shape invariants on the variant).
  """
  let seed: U64
  let payload_string: Bool
  let payload_size: USize
  let payload_fresh: Bool
  let workload: (_Mesh | _Cyclic | _Backpressure)

  new val create(seed': U64, payload_string': Bool, payload_size': USize,
    payload_fresh': Bool, workload': (_Mesh | _Cyclic | _Backpressure))
  =>
    seed = seed'
    payload_string = payload_string'
    payload_size = payload_size'
    payload_fresh = payload_fresh'
    workload = workload'

primitive _Cli
  """
  Builds the workload command spec and extracts a validated `_Config` from a
  parsed command. The `cli` package handles unknown flags, missing values, bad
  numbers, and `--help`; this adds the value-set and lower-bound checks it can't
  express. Runtime `--pony*` flags are stripped by the runtime before `Main` sees
  `env.args`, so they never reach the parser. Every option carries a default, so a
  caller (e.g. the systematic orchestrator) that omits a workload's specific flags
  still parses cleanly as a `mesh` run. The CLI spec is deliberately flat -- every
  flag is accepted for every run, with "ignored for ..." help text -- while the
  validated `_Config` it builds is a per-kind union; only the union enforces that a
  workload carries only its own shape fields.
  """
  fun spec(): CommandSpec ? =>
    CommandSpec.leaf("generative",
      "Generative runtime stress workload (driven by the orchestrate_*.py harnesses)",
      [
        OptionSpec.u64("seed",
          "engine RNG seed"
          where default' = 1)
        OptionSpec.string("workload",
          "workload kind: mesh | cyclic | backpressure"
          where default' = "mesh")
        OptionSpec.u64("pingers",
          "mesh only (ignored otherwise): number of mesh actors (>= 1)"
          where default' = 8)
        OptionSpec.u64("generations",
          "cyclic only (ignored otherwise): garbage-group generations (>= 1)"
          where default' = 1)
        OptionSpec.u64("group",
          "cyclic only (ignored otherwise): actors per garbage group (>= 2)"
          where default' = 2)
        OptionSpec.u64("producers",
          "backpressure only (ignored otherwise): flooding producers (>= 1)"
          where default' = 64)
        OptionSpec.u64("messages",
          "backpressure only (ignored otherwise): work msgs per producer (>= 1)"
          where default' = 1000)
        OptionSpec.u64("apply-every",
          "backpressure only (ignored otherwise): received msgs between "
          + "apply/release toggles (>= 1)"
          where default' = 200)
        OptionSpec.u64("chains",
          "mesh/cyclic only (ignored for backpressure): chains to inject (>= 1)"
          where default' = 8)
        OptionSpec.u64("ttl",
          "mesh/cyclic only (ignored for backpressure): hops per chain"
          where default' = 16)
        OptionSpec.string("payload",
          "traced payload kind: string | u64"
          where default' = "string")
        OptionSpec.u64("payload-size",
          "string payload size in bytes (>= 1)"
          where default' = 64)
        OptionSpec.string("payload-mode",
          "per-send payload allocation: forward | fresh"
          where default' = "forward")
      ])?.>add_help()?

  fun config(cmd: Command): (_Config | String) =>
    let seed = cmd.option("seed").u64()
    let chains = cmd.option("chains").u64().usize()
    let ttl = cmd.option("ttl").u64().usize()
    let payload_size = cmd.option("payload-size").u64().usize()
    let payload = cmd.option("payload").string()
    let mode = cmd.option("payload-mode").string()
    let workload_name = cmd.option("workload").string()
    let pingers = cmd.option("pingers").u64().usize()
    let generations = cmd.option("generations").u64().usize()
    let group = cmd.option("group").u64().usize()
    let producers = cmd.option("producers").u64().usize()
    let messages = cmd.option("messages").u64().usize()
    let apply_every = cmd.option("apply-every").u64().usize()

    let payload_string =
      if payload == "string" then
        true
      elseif payload == "u64" then
        false
      else
        return "bad --payload (expected 'string' or 'u64')"
      end

    let payload_fresh =
      if mode == "forward" then
        false
      elseif mode == "fresh" then
        true
      else
        return "bad --payload-mode (expected 'forward' or 'fresh')"
      end

    if payload_string and (payload_size < 1) then
      return "--payload-size must be >= 1 for string payload"
    end

    // chains is validated only where it is used (the two chain-based workloads);
    // backpressure has no chains, so it carries none.
    let workload: (_Mesh | _Cyclic | _Backpressure) =
      if workload_name == "mesh" then
        if pingers < 1 then return "--pingers must be >= 1" end
        if chains < 1 then return "--chains must be >= 1" end
        _Mesh(pingers, chains, ttl)
      elseif workload_name == "cyclic" then
        if generations < 1 then return "--generations must be >= 1" end
        if group < 2 then return "--group must be >= 2" end
        if chains < 1 then return "--chains must be >= 1" end
        _Cyclic(generations, group, chains, ttl)
      elseif workload_name == "backpressure" then
        if producers < 1 then return "--producers must be >= 1" end
        if messages < 1 then return "--messages must be >= 1" end
        if apply_every < 1 then return "--apply-every must be >= 1" end
        _Backpressure(producers, messages, apply_every)
      else
        return "bad --workload (expected 'mesh', 'cyclic' or 'backpressure')"
      end

    _Config(seed, payload_string, payload_size, payload_fresh, workload)

primitive _Fatal
  """
  The harness's runtime-fault detector: print a diagnostic to stderr and exit
  non-zero. Used by the late/duplicate-message oracles across all workloads -- a
  ping arriving after a `Pinger` has reported, a cyclic completion beyond the
  expected total, or a backpressure `work`/`finished` after the `Consumer` is done
  (or more `finished` reports than producers) -- each is a duplicated or delayed
  message, a real runtime fault, so the stress run fails loudly with a location
  rather than passing.
  """
  fun apply(msg: String, loc: SourceLoc = __loc) =>
    let line = "FATAL: " + msg + " (" + loc.file() + ":"
      + loc.line().string() + ")\n"
    @fprintf(@pony_os_stderr(), "%s".cstring(), line.cstring())
    @exit(I32(1))

primitive _Unreachable
  """
  A branch the compiler can't prove is dead but we know is: every guarded array
  access here is indexed by `Rand.int(size)` with `size >= 1`, so the index is in
  range by construction. If one ever fires it is a real bug, so crash with a
  location rather than continue with corrupt state (the "unreachable try/else"
  rule). Distinct from `_Fatal`: this flags a fault that should be impossible,
  not one the harness is built to catch.
  """
  fun apply(msg: String, loc: SourceLoc = __loc) =>
    let line = "UNREACHABLE: " + msg + " (" + loc.file() + ":"
      + loc.line().string() + ")\n"
    @fprintf(@pony_os_stderr(), "%s".cstring(), line.cstring())
    @exit(I32(1))
