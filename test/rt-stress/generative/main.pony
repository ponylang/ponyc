"""
Generative runtime stress harness.

A swarm-driven Pony program that exercises the runtime's message passing, ORCA
tracing, the cycle detector's collection path (via the `cyclic` workload), the
explicit backpressure / muting path (via the `backpressure` workload), ORCA's
transfer of ownership of a nested mutable object subgraph between actors (via the
`iso` workload), and ORCA's actor-reference counting -- the acquire / release of
actor `tag`s passed as message cargo (via the `actorref` workload). One run draws
one of five closed, count-driven workloads (`--workload`):

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

* `iso` (M1): the `mesh` topology, but the shared `val` payload is replaced by a
  freshly built nested `iso` object graph -- a `Parcel` tree of `--node-depth`
  levels with `--node-breadth` children per node, every node a separate allocation
  holding its own byte array -- `consume`d hop-to-hop. Each hop moves ownership of
  the whole mutable subgraph to the next actor, so the receiver acquires a foreign
  *mutable* multi-object graph node by node -- a trace path no other workload
  reaches (the others only ever make a raw byte buffer mutable, never a typed
  object struct, and never transfer a mutable subgraph). The swarm draws the graph
  shape, so runs span a single flat node through a 15-node tree. At the terminal
  hop the graph is consumed into a `ref` and every node's sentinel bytes are
  verified, so a silent GC corruption or premature free of the moved subgraph is
  caught, not just a crash. Conservation is the mesh's full send/receive tally
  (`sent == received == chains * (ttl + 1)`); the `Carrier`s are live at quiescence
  (the `Dispatcher` holds them), so they are polled exactly like the mesh.

* `actorref` (M1): the `mesh` topology, but the cargo is a `Referent tag` -- a
  reference to a bare actor -- instead of a `val` payload. A FRESH `Referent` is
  built per injected chain and its tag rides the chain hop-to-hop; each `Relay`
  forwards the tag onward and NEVER stores it. A freshly received foreign actor
  reference sits at reference count 1, so forwarding it exhausts its weighted
  budget on that send -- the forward drives ORCA's actor-reference ACQUIRE, and
  dropping it (never stored) drives the RELEASE -- so `send_remote_actor` /
  `recv_remote_actor` / `acquire_actor` fire proportional to `chains * ttl`, the
  actor-reference machinery no other workload exercises under load (measured: the
  others hit `acquire_actor` 0-1 times a run, all at setup). A fixed referent pool
  was rejected by measurement -- a shared referent's weighted budget stays high, so
  forwards stop acquiring and the path goes cold; fresh-per-chain also makes a lost
  release leak in proportion to `chains`. Conservation is the mesh's full
  send/receive tally (`sent == received == chains * (ttl + 1)`); the `Relay`s are
  live at quiescence (the `Referrer` holds them), so they are polled exactly like
  the mesh, while the referents become garbage as their chains drain. See
  .known-couplings/actorref-workload-actor-ref-cargo.md.

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
folds its scheduler-visible arrivals -- chain completions for mesh/cyclic/iso; for
backpressure, every work arrival (the mute/unmute interleaving) plus the producer
`finished` reports -- into an FNV-1a `ORDER_SIG`, a pure function of the scheduler
interleaving and the observable for the determinism oracle.

This program holds no `@runtime_override_defaults`: every runtime knob (scaling,
cycle detector, GC, thread count, the systematic seed) is a `--pony*` flag set by
the orchestrator, and every workload parameter is a `--flag value` arg parsed
here. Two orchestrators in this directory drive it: `orchestrate_systematic.py`
(serialized, reproducible -- draws all five workloads, with `--ponynoblock` drawn as
a swarm knob so the cycle detector runs under the oracle on the runs where it is
absent) and `orchestrate_normal.py` (a normal, real-parallel runtime -- also draws
all five workloads; the conservation invariant holds there too, but `ORDER_SIG` does
not reproduce, so that mode runs each seed once).
"""
use "backpressure"
use "cli"
use "random"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @fflush[I32](stream: Pointer[U8] tag)
use @pony_os_stdout[Pointer[U8]]()
use @pony_os_stderr[Pointer[U8]]()
use @exit[None](status: I32)

type Payload is (String val | U64)
  """
  A message's traced cargo: a heap-allocated `String val` (gives ORCA something to
  trace across the actor boundary) or a `U64` primitive (a no-GC control). The
  kind is fixed for a whole run by `--payload`; `--payload-mode` then decides
  whether one payload object is reused (forwarded the whole chain, or re-sent by a
  producer) or a fresh one is allocated at each send. Shared by the mesh, cyclic,
  and backpressure workloads (the `iso` workload has its own `Parcel` cargo).
  """

primitive _Payloads
  """
  Builds a message payload of the configured kind. `--payload-mode forward` reuses
  one object across a sender's sends (exercising ORCA's shared-`val` refcounting);
  `fresh` calls this at every send (exercising allocation + tracing + collection
  churn). Used by the mesh, cyclic, and backpressure workloads (the `iso` workload
  builds its own `Parcel` cargo instead).
  """
  fun make(config: _Config): Payload =>
    if config.payload_string then
      let s: String val =
        String.from_array(recover val Array[U8].init('p', config.payload_size) end)
      s
    else
      U64(42)
    end

primitive _Heartbeat
  """
  The no-progress watchdog's progress signal. A receiving actor calls `emit` every
  `interval(...)` messages it handles; the normal orchestrator's watchdog reads
  these to tell a slow-but-advancing run from a hung one, killing only on silence
  (see stress_common.py). The line carries no data -- only its arrival matters, and
  it adds no message and no fan-out, so the conservation and ORDER_SIG oracles are
  untouched.

  Flushing is load-bearing: a bare @printf to a piped stdout is block-buffered, so
  the line would not reach the watchdog until the buffer fills or the program exits,
  defeating the watchdog. @fflush forces it out per heartbeat (verified to arrive in
  real time both directly and under lldb).
  """
  fun interval(total_messages: USize, emitters: USize): USize =>
    """
    Per-emitter message count between heartbeats, or 0 to disable. Disabled when the
    run is small enough to finish well within the watchdog's no-progress window (so
    it needs no heartbeat -- the cyclic workload and small mesh/backpressure draws);
    otherwise ~`_target` heartbeats per emitter, floored at 1. COUPLING: this cadence
    is paired with the orchestrator's no_progress threshold -- a live run must
    heartbeat well within it; see .known-couplings/stress-heartbeat-watchdog.md.
    """
    if (emitters == 0) or (total_messages < _min()) then
      0
    else
      (total_messages / (emitters * _target())).max(1)
    end

  fun emit() =>
    @printf("HEARTBEAT\n".cstring())
    @fflush(@pony_os_stdout())

  fun _target(): USize => 50
  fun _min(): USize => 1_000_000

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
        | let iso': _Iso => Dispatcher(config, iso')
        | let ar: _ActorRef => Referrer(config, ar)
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
    // Per-Pinger heartbeat cadence: ~_target prints per Pinger over the run, off
    // for runs too small to need a progress signal. Computed once and shared.
    let hb = _Heartbeat.interval(mesh.chains * (mesh.ttl + 1), mesh.pingers)
    let ps: Array[Pinger] iso = recover Array[Pinger](mesh.pingers) end
    var id: USize = 0
    while id < mesh.pingers do
      ps.push(Pinger(this, id, config, hb))
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
    its final counts. A completion beyond `chains` is a duplicate or late message
    -- a real fault -- so it trips `_Fatal`.
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
    elseif _completions > _mesh.chains then
      _Fatal("late/duplicate completion (completions > chains)")
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
  let _hb_interval: USize
  var _neighbors: Array[Pinger] val
  var _received: U64 = 0
  var _sent: U64 = 0
  var _reported: Bool = false
  let _rand: Rand

  new create(coord: Coordinator, id: USize, config: _Config, hb_interval: USize) =>
    _coord = coord
    _id = id
    _config = config
    _hb_interval = hb_interval
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
    if (_hb_interval > 0) and ((_received % _hb_interval.u64()) == 0) then
      _Heartbeat.emit()
    end
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
    _consumer.work(_id, outgoing)
    _sent = _sent + 1
    _remaining = _remaining - 1
    produce()

actor Consumer
  """
  The `backpressure` workload's driver, sink, conservation oracle, and ORDER_SIG
  collector in one actor (mirroring how the mesh's `Coordinator` both drives and
  evaluates). It spawns the producers, counts received work, participates in runtime
  backpressure, folds each arrival into `ORDER_SIG` (see `work`/`finished`), and on
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
  let _hb_interval: USize
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
    // Single consumer, so it is the sole heartbeat emitter for this workload.
    _hb_interval = _Heartbeat.interval(bp.producers * bp.messages, 1)

    // Spawn the producers pointing at this consumer (the Coordinator pattern: the
    // driver creates the workers with `this`). Each floods `messages` work
    // messages and then reports `finished`.
    var id: USize = 0
    while id < bp.producers do
      Producer(this, config, id, bp.messages)
      id = id + 1
    end

  be work(id: USize, payload: Payload) =>
    """
    Count one received work message and run the backpressure hysteresis. The
    payload is traced by ORCA across the send/receive crossing and then dropped
    (the consumer doesn't retain it). A work after completion is a leaked/late
    message -- a real fault -- so it trips `_Fatal`.

    The sending producer's `id` is folded into `ORDER_SIG` on every arrival: a
    work send is this workload's muting point, so the sequence of arrivals at the
    consumer IS the mute/unmute interleaving. Fingerprinting it per message (not
    just the terminal `finished` reports) is what makes the determinism oracle
    sensitive to a muting-induced reordering -- the whole reason this workload is
    worth running under systematic replay.
    """
    if _done then
      _Fatal("work after done (leaked/late message)")
    end
    _received = _received + 1
    _mix(id.u64())
    if (_hb_interval > 0) and ((_received % _hb_interval.u64()) == 0) then
      _Heartbeat.emit()
    end
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
    Fold one producer's terminal report into `ORDER_SIG` (the producer-`finished`
    race, folded on top of the per-arrival work fold above) and accumulate its send
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

class Parcel
  """
  The `iso` workload's cargo: a multi-object MUTABLE subgraph, shaped by the swarm.
  Each node is a typed struct (traced `PONY_TRACE_MUTABLE` while owned through an
  `iso`) holding its own byte array AND an array of child `Parcel`s, so one
  `Parcel iso` is a tree of `depth` levels, `breadth` children per node, every node
  separately allocated. Built fresh per injected chain and moved (`consume`d) at
  each hop; never reused (an `iso` is single-owner). Every byte is the sentinel 'p',
  so the terminal hop can verify the whole moved subgraph survived uncorrupted.

  `bytes` and `kids` are `let`, NOT `embed`, on purpose -- separate heap
  allocations, so a receiver acquires each node as a distinct foreign mutable
  object. An `embed` would co-allocate them, collapsing the multi-object subgraph
  this workload exists to trace. The workload's coverage rests on this type being
  built and handed off as `iso` (not `val`) and staying multi-object (`let`, not
  `embed`): changing either silently loses the coverage while every oracle stays
  green -- see .known-couplings/iso-handoff-workload-ownership.md (pony-ref's
  "prefer embed" guidance does not apply here).
  """
  let bytes: Array[U8]
  let kids: Array[Parcel]

  new create(size: USize, depth: USize, breadth: USize) =>
    bytes = Array[U8].init('p', size)
    // A node has `breadth` children only when it is not the last level; the last
    // level (depth == 1) has none. `depth == 1` or `breadth == 0` is a single node.
    let n = if depth > 1 then breadth else 0 end
    kids = Array[Parcel](n)
    var i: USize = 0
    while i < n do
      kids.push(Parcel(size, depth - 1, breadth))
      i = i + 1
    end

actor Carrier
  """
  An `iso`-workload node. Mirrors `Pinger`: produces exactly one successor message
  per received handoff and tracks how many it has sent and received. The successor
  carries the `consume`d parcel itself -- moving ownership of the mutable subgraph
  one hop further -- so the trace path the workload exists to drive runs on every
  forward. At the terminal hop it verifies the parcel before dropping it.
  """
  let _dispatcher: Dispatcher
  let _id: USize
  var _neighbors: Array[Carrier] val
  var _received: U64 = 0
  var _sent: U64 = 0
  var _reported: Bool = false
  let _rand: Rand

  new create(dispatcher: Dispatcher, id: USize, seed: U64) =>
    _dispatcher = dispatcher
    _id = id
    _neighbors = recover val Array[Carrier] end
    // Per-Carrier deterministic draw stream, seeded only from `--seed` + id (the
    // same scheme as `Pinger`); which value lands on which handoff depends on
    // mailbox arrival order, so routing is a function of the seed AND the
    // interleaving. No `_Config` is stored: a `Carrier` needs nothing from it but
    // the seed (it has no payload logic, unlike `Pinger`).
    _rand = Rand(seed, id.u64())

  be set_neighbors(neighbors: Array[Carrier] val) =>
    """
    Install the full mesh BEFORE any handoff can arrive. The `Dispatcher` sends
    this to every `Carrier` in program order before it injects, so by Pony's
    causal delivery every `Carrier` has its neighbors before any handoff -- the
    same load-bearing ordering as `Pinger.set_neighbors`.
    """
    _neighbors = neighbors

  be carry(parcel: Parcel iso, hops: USize, chain: USize) =>
    """
    Produce exactly one successor per received handoff: a forward of the
    `consume`d parcel when hops > 0, otherwise verify it and signal completion.
    This one-in/one-out invariant is what makes the dispatcher's completion count a
    sound quiescence barrier (module docstring) -- do not make this fan out.
    `consume` moves the parcel; it is never aliased, so the U64 tallies are
    untouched by the move.
    """
    if _reported then
      // Quiescence is proven before `report()` is sent, so no handoff should
      // arrive afterwards. One that does is a leaked or duplicated message.
      _Fatal("carry after report (leaked/late message)")
    end
    _received = _received + 1
    if hops > 0 then
      _sent = _sent + 1
      let next = _rand.int(_neighbors.size().u64()).usize()
      try
        _neighbors(next)?.carry(consume parcel, hops - 1, chain)
      else
        _Unreachable("carrier neighbor index out of range")
      end
    else
      // Terminal hop. Consume the parcel into a `ref` so its arrays can be read
      // (a field read THROUGH an `iso` viewpoint-adapts to `tag` and will not
      // compile; consuming into `ref` gives full read access, since `iso^` can
      // become `ref`). Verify it, then drop it.
      let p: Parcel ref = consume parcel
      _verify(p)
      _dispatcher.completed(chain, _id, _received)
    end

  fun _verify(parcel: Parcel box) =>
    """
    Integrity oracle (recursive): every byte of every node in the graph was the
    sentinel 'p' at construction and the subgraph was only moved (never mutated)
    since, so each node's first and last byte must still be 'p'. Walk the whole
    tree (`node_size >= 1` is a `_Config` invariant, so the endpoint reads are in
    range). A mismatch is a silent GC/ORCA corruption or premature free of the
    moved subgraph -- a real fault none of the message-counting oracles can see.
    """
    try
      let n = parcel.bytes.size()
      if (parcel.bytes(0)? != 'p') or (parcel.bytes(n - 1)? != 'p') then
        _Fatal("parcel corruption (sentinel byte mismatch)")
      end
    else
      _Unreachable("parcel array empty (node_size >= 1 guaranteed)")
    end
    for kid in parcel.kids.values() do
      _verify(kid)
    end

  be report() =>
    _reported = true
    _dispatcher.report_counts(_sent, _received)

actor Dispatcher
  """
  The `iso` workload's driver, injector, and conservation oracle in one actor (the
  mesh `Coordinator` pattern). Builds the `Carrier` mesh, injects the parcels,
  counts completions to detect quiescence, collects per-`Carrier` send/receive
  tallies, and evaluates conservation. On completion it prints `ORDER_SIG` plus the
  tallies; on a conservation failure it forces a non-zero exit, otherwise it
  returns and lets the program quiesce.
  """
  let _iso: _Iso
  let _carriers: Array[Carrier] val
  var _completions: USize = 0
  var _reports_outstanding: USize = 0
  var _total_sent: U64 = 0
  var _total_received: U64 = 0
  var _done: Bool = false
  var _sig: U64 = 14695981039346656037 // FNV-1a 64-bit offset basis

  new create(config: _Config, iso': _Iso) =>
    _iso = iso'

    // Build the mesh. Carriers are created outside the `recover` block (so we can
    // pass `this`) and pushed into the iso array via automatic receiver recovery
    // -- `push`'s argument, a `Carrier tag`, is sendable. Only `config.seed` is
    // needed (in this constructor), so no `_Config` field is stored.
    let cs: Array[Carrier] iso = recover Array[Carrier](iso'.pingers) end
    var id: USize = 0
    while id < iso'.pingers do
      cs.push(Carrier(this, id, config.seed))
      id = id + 1
    end
    let carriers: Array[Carrier] val = consume cs
    _carriers = carriers

    // Wire neighbors, THEN inject. The ordering is load-bearing -- the same
    // reasoning as `Coordinator` (see `Carrier.set_neighbors`).
    for c in carriers.values() do
      c.set_neighbors(carriers)
    end

    // Each injected parcel is one dispatcher send (added back in `_finish`).
    let r = Rand(config.seed)
    var chain: USize = 0
    while chain < iso'.chains do
      let start = r.int(carriers.size().u64()).usize()
      try
        // Fresh graph per chain -- an `iso` is single-owner, so there is no
        // forward-mode reuse; the graph is always newly built and moved.
        carriers(start)?.carry(
          recover iso Parcel(iso'.node_size, iso'.depth, iso'.breadth) end,
          iso'.ttl, chain)
      else
        _Unreachable("iso injection start index out of range")
      end
      chain = chain + 1
    end

  be completed(chain: USize, terminal_id: USize, received_snapshot: U64) =>
    """
    A parcel reached its terminal (hops == 0) carrier. Fold the arrival into the
    order signature; once every parcel has terminated the system is quiesced, so
    ask every `Carrier` for its final counts. A completion beyond the expected
    total is a duplicate or late message -- a real fault -- so it trips `_Fatal`.
    """
    if _done then _Fatal("completion after done (leaked/late message)") end
    _mix(chain.u64())
    _mix(terminal_id.u64())
    _mix(received_snapshot)
    _completions = _completions + 1
    if _completions == _iso.chains then
      _reports_outstanding = _carriers.size()
      for c in _carriers.values() do c.report() end
    elseif _completions > _iso.chains then
      _Fatal("late/duplicate completion (completions > chains)")
    end

  be report_counts(sent: U64, received: U64) =>
    """
    Accumulate one `Carrier`'s tallies. When all have reported, evaluate the
    conservation oracle.
    """
    _total_sent = _total_sent + sent
    _total_received = _total_received + received
    _reports_outstanding = _reports_outstanding - 1
    if _reports_outstanding == 0 then
      _finish()
    end

  fun ref _finish() =>
    _done = true
    // The dispatcher itself performed `chains` initial sends (the injected
    // parcels); every `Carrier` forward is already in `_total_sent`.
    let total_sent = _total_sent + _iso.chains.u64()
    let expected = _iso.chains.u64() * (_iso.ttl.u64() + 1)
    let conserved =
      (total_sent == _total_received) and (_total_received == expected)

    // Synchronous print so the result survives natural quiescence (env.out.print
    // is async and could be lost if a later exit raced it).
    let line = "RECEIVED=" + _total_received.string()
      + " SENT=" + total_sent.string()
      + " EXPECTED=" + expected.string()
      + " ORDER_SIG=" + _sig.string() + "\n"
    @printf("%s".cstring(), line.cstring())

    // On success: return and let the program reach natural quiescence (no forced
    // exit), same as the other workloads. Only a conservation FAILURE forces a
    // non-zero exit: fail fast once a bug is detected.
    if not conserved then
      let diag = "CONSERVATION FAILURE: received=" + _total_received.string()
        + " sent=" + total_sent.string()
        + " expected=" + expected.string() + "\n"
      @fprintf(@pony_os_stderr(), "%s".cstring(), diag.cstring())
      @exit(I32(1))
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211 // FNV-1a 64-bit prime

actor Referent
  """
  The `actorref` workload's cargo: a bare actor whose `tag` is the traced
  reference the relays churn. It has no fields and no behaviors -- it exists only
  to BE referenced. Passing its tag across an actor boundary drives ORCA's
  actor-reference machinery (`ponyint_gc_sendactor` / `recv_remote_actor` /
  `acquire_actor` in `src/libponyrt/gc/gc.c`); the runtime's message loop handles
  the ACQUIRE / RELEASE system messages, so no user code is needed. A fresh
  `Referent` is built per injected chain (see `Referrer`) and referenced only by
  that chain's in-flight `cite` plus, transiently, its creator -- so it becomes
  garbage as its chain drains, which is what makes a premature free (crash) or a
  lost release (memory growth) observable. See
  .known-couplings/actorref-workload-actor-ref-cargo.md.
  """
  new create() => None

actor Relay
  """
  A node in the `actorref` routing mesh. Mirrors `Pinger`: forwards exactly one
  successor `cite` per received `cite` (one-in/one-out) and tracks how many it has
  sent and received. The cargo is a `Referent tag` it NEVER stores -- it re-sends
  the just-received tag when hops remain, or drops it at the terminal. A freshly
  received foreign actor reference sits at reference count 1, so forwarding it
  exhausts the referent's weighted budget on that send, and the forward drives
  `acquire_actor` -- the actor-reference path no other workload reaches under load.
  Not storing the referent is load-bearing: it lets the reference drop so the next
  hop acquires afresh. Holding the neighbor RELAYS permanently is fine -- a
  neighbor is a `cite` recipient, not traced cargo -- only the cargo referent must
  stay unheld (see .known-couplings/actorref-workload-actor-ref-cargo.md). Stays
  live at quiescence so the `Referrer` polls it (the mesh's strong tally).
  """
  let _referrer: Referrer
  let _id: USize
  let _hb_interval: USize
  var _neighbors: Array[Relay] val
  var _received: U64 = 0
  var _sent: U64 = 0
  var _reported: Bool = false
  let _rand: Rand

  new create(referrer: Referrer, id: USize, seed: U64, hb_interval: USize) =>
    _referrer = referrer
    _id = id
    _hb_interval = hb_interval
    _neighbors = recover val Array[Relay] end
    // Per-Relay deterministic draw stream, seeded only from `--seed` + id (the
    // same scheme as `Pinger`/`Carrier`): the value stream is fixed by the seed,
    // but which value lands on which cite depends on mailbox arrival order, so
    // routing is a function of the seed AND the interleaving. No `_Config` is
    // stored -- a `Relay` has no payload logic (its cargo is the passed tag),
    // like `Carrier`.
    _rand = Rand(seed, id.u64())

  be set_neighbors(neighbors: Array[Relay] val) =>
    """
    Install the routing mesh BEFORE any cite can arrive (causal ordering, the same
    load-bearing reasoning as `Pinger.set_neighbors`). This holds RELAYS, not the
    cargo referent, so holding it permanently is fine -- see the class docstring for
    why only the cargo referent must stay unheld.
    """
    _neighbors = neighbors

  be cite(referent: Referent tag, hops: USize, chain: USize) =>
    """
    Forward exactly one successor cite carrying the same referent tag when
    hops > 0, else drop the referent and signal completion. The referent is NEVER
    stored: re-sending the just-received tag (weighted budget 1) is what fires
    `acquire_actor` -- the whole point of this workload. One-in/one-out keeps the
    completion count a sound quiescence barrier (module docstring) -- do not fan
    out.
    """
    if _reported then
      // Quiescence is proven before `report()` is sent, so no cite should arrive
      // afterwards. One that does is a leaked or duplicated message.
      _Fatal("cite after report (leaked/late message)")
    end
    _received = _received + 1
    if (_hb_interval > 0) and ((_received % _hb_interval.u64()) == 0) then
      _Heartbeat.emit()
    end
    if hops > 0 then
      _sent = _sent + 1
      let next = _rand.int(_neighbors.size().u64()).usize()
      try
        _neighbors(next)?.cite(referent, hops - 1, chain)
      else
        _Unreachable("relay neighbor index out of range")
      end
    else
      _referrer.completed(chain, _id, _received)
    end

  be report() =>
    _reported = true
    _referrer.report_counts(_sent, _received)

actor Referrer
  """
  The `actorref` workload driver (the mesh `Dispatcher` pattern). Builds the
  `Relay` routing mesh (relays stay LIVE -- held in `_relays` -- so they can be
  polled: the mesh's strong conservation tally), then injects `chains` chains, each
  carrying a FRESH `Referent`'s tag as cargo down the mesh with a hop count (TTL).
  Counts completions to detect quiescence, collects per-`Relay` send/receive
  tallies, folds `ORDER_SIG`, and evaluates conservation; on completion it prints
  the result and returns (natural quiescence), forcing a non-zero exit only on a
  conservation failure. Why the referent is built FRESH per chain (not drawn from a
  fixed pool) and never stored is the workload's load-bearing coverage property --
  see the injection loop below and
  .known-couplings/actorref-workload-actor-ref-cargo.md.
  """
  let _actorref: _ActorRef
  let _relays: Array[Relay] val
  var _completions: USize = 0
  var _reports_outstanding: USize = 0
  var _total_sent: U64 = 0
  var _total_received: U64 = 0
  var _done: Bool = false
  var _sig: U64 = 14695981039346656037 // FNV-1a 64-bit offset basis

  new create(config: _Config, actorref: _ActorRef) =>
    _actorref = actorref

    // Per-Relay heartbeat cadence: ~_target prints per Relay over the run, off for
    // runs too small to need a progress signal. Computed once and shared.
    let hb = _Heartbeat.interval(actorref.chains * (actorref.ttl + 1),
      actorref.relays)

    // Build the relay mesh. Relays are held in a field (_relays) so they are LIVE
    // at quiescence and can be polled -- the mesh's strong send/recv tally. Built
    // outside the `recover` block (so we can pass `this`) and pushed via automatic
    // receiver recovery -- a `Relay tag` is sendable.
    let rs: Array[Relay] iso = recover Array[Relay](actorref.relays) end
    var id: USize = 0
    while id < actorref.relays do
      rs.push(Relay(this, id, config.seed, hb))
      id = id + 1
    end
    let relays: Array[Relay] val = consume rs
    _relays = relays

    // Wire neighbors, THEN inject. The ordering is load-bearing -- the same
    // reasoning as `Coordinator` (see `Relay.set_neighbors`).
    for r in relays.values() do
      r.set_neighbors(relays)
    end

    // Inject. Each chain gets a FRESH `Referent`, whose tag rides the whole chain
    // as cargo. A relay forwarding a just-received referent it does not hold
    // exhausts the weighted budget on the first send, so the forward drives
    // `acquire_actor` -- the cold path this workload exists to exercise. The fresh
    // referent is NOT retained here (only the in-flight cite references it), and
    // this loop's continued allocation makes the Referrer sweep and release
    // earlier referents, so each becomes garbage as its chain drains. Each
    // injected cite is one Referrer send (added back in `_finish`).
    let r = Rand(config.seed)
    var chain: USize = 0
    while chain < actorref.chains do
      let start = r.int(relays.size().u64()).usize()
      try
        relays(start)?.cite(Referent, actorref.ttl, chain)
      else
        _Unreachable("actorref injection start index out of range")
      end
      chain = chain + 1
    end

  be completed(chain: USize, terminal_id: USize, received_snapshot: U64) =>
    """
    A chain reached its terminal (hops == 0) relay. Fold the arrival into the order
    signature; once every chain has terminated the system is quiesced, so ask every
    `Relay` for its final counts. A completion beyond `chains` is a duplicate or
    late message -- a real fault -- so it trips `_Fatal`.
    """
    if _done then _Fatal("completion after done (leaked/late message)") end
    _mix(chain.u64())
    _mix(terminal_id.u64())
    _mix(received_snapshot)
    _completions = _completions + 1
    if _completions == _actorref.chains then
      _reports_outstanding = _relays.size()
      for r in _relays.values() do r.report() end
    elseif _completions > _actorref.chains then
      _Fatal("late/duplicate completion (completions > chains)")
    end

  be report_counts(sent: U64, received: U64) =>
    """
    Accumulate one `Relay`'s tallies. When all have reported, evaluate the
    conservation oracle.
    """
    _total_sent = _total_sent + sent
    _total_received = _total_received + received
    _reports_outstanding = _reports_outstanding - 1
    if _reports_outstanding == 0 then _finish() end

  fun ref _finish() =>
    _done = true
    // The Referrer itself performed `chains` initial sends (the injected cites);
    // every `Relay` forward is already in `_total_sent`.
    let total_sent = _total_sent + _actorref.chains.u64()
    let expected = _actorref.chains.u64() * (_actorref.ttl.u64() + 1)
    let conserved =
      (total_sent == _total_received) and (_total_received == expected)

    // Synchronous print so the result survives natural quiescence (env.out.print
    // is async and could be lost if a later exit raced it).
    let line = "RECEIVED=" + _total_received.string()
      + " SENT=" + total_sent.string()
      + " EXPECTED=" + expected.string()
      + " ORDER_SIG=" + _sig.string() + "\n"
    @printf("%s".cstring(), line.cstring())

    // On success: return and let the program reach natural quiescence (no forced
    // exit), same as the other workloads. Only a conservation FAILURE forces a
    // non-zero exit: fail fast once a bug is detected.
    if not conserved then
      let diag = "CONSERVATION FAILURE: received=" + _total_received.string()
        + " sent=" + total_sent.string()
        + " expected=" + expected.string() + "\n"
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
  structurally absent here (the reason chains/ttl live on `_Mesh`/`_Cyclic`/`_Iso`
  rather than on the shared `_Config`).
  """
  let producers: USize
  let messages: USize
  let apply_every: USize

  new val create(producers': USize, messages': USize, apply_every': USize) =>
    producers = producers'
    messages = messages'
    apply_every = apply_every'

class val _Iso
  """
  The `iso` workload's shape. Built only by `_Cli.config` (after validation), so
  `pingers >= 1`, `chains >= 1`, `node_size >= 1`, and `depth >= 1` can be trusted
  (`ttl` and `breadth` have no lower bound -- breadth 0 or depth 1 is a single-node
  graph). Carries its own shape for the same reason as `_Mesh`/`_Cyclic`: a
  `backpressure` config cannot carry it, so an illegal cross-workload field mix is
  unrepresentable. Has its OWN cargo knobs (node_size/depth/breadth shape the iso
  graph), NOT the shared String-payload knobs -- an `iso` can't ride the
  `(String val | U64)` Payload.
  """
  let pingers: USize
  let chains: USize
  let ttl: USize
  let node_size: USize
  let depth: USize
  let breadth: USize

  new val create(pingers': USize, chains': USize, ttl': USize,
    node_size': USize, depth': USize, breadth': USize)
  =>
    pingers = pingers'
    chains = chains'
    ttl = ttl'
    node_size = node_size'
    depth = depth'
    breadth = breadth'

class val _ActorRef
  """
  The `actorref` workload's shape. Built only by `_Cli.config` (after validation),
  so `relays >= 1` and `chains >= 1` can be trusted (`ttl` has no lower bound at the
  CLI, though the orchestrators draw `ttl >= 1` -- a zero-hop (ttl=0) chain injects
  but never forwards, so it drives no acquire). Its fields match `_Mesh`'s
  (relays/chains/ttl are a routing mesh with a hop count), but it is a DISTINCT type
  on purpose: `Main` dispatches on the variant type to pick `Referrer` vs
  `Coordinator`, and the two workloads carry different semantics (actor-`tag` cargo
  vs a `(String val | U64)` payload). Carries its own shape for the same reason as
  `_Mesh`/`_Iso`. Has NO payload knobs -- its cargo is a fresh actor per chain, not
  a `(String val | U64)` payload, so like `iso` it does not ride the shared payload
  union.
  """
  let relays: USize
  let chains: USize
  let ttl: USize

  new val create(relays': USize, chains': USize, ttl': USize) =>
    relays = relays'
    chains = chains'
    ttl = ttl'

class val _Config
  """
  A validated workload configuration: the fields shared by all workloads (seed and
  payload kind/size/mode) plus the workload-specific shape (`_Mesh`, `_Cyclic`,
  `_Backpressure`, `_Iso`, or `_ActorRef`). The chain/hop counts are NOT here --
  only the four chain-based workloads (mesh/cyclic/iso/actorref) have them, so they
  live on those variants. Built only by `_Cli.config`, so the rest of the program
  can trust its invariants (payload_size >= 1 when payload_string, plus the
  per-shape invariants on the variant).
  """
  let seed: U64
  let payload_string: Bool
  let payload_size: USize
  let payload_fresh: Bool
  let workload: (_Mesh | _Cyclic | _Backpressure | _Iso | _ActorRef)

  new val create(seed': U64, payload_string': Bool, payload_size': USize,
    payload_fresh': Bool,
    workload': (_Mesh | _Cyclic | _Backpressure | _Iso | _ActorRef))
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
          "workload kind: mesh | cyclic | backpressure | iso | actorref"
          where default' = "mesh")
        OptionSpec.u64("pingers",
          "mesh/iso/actorref only (ignored otherwise): number of mesh actors "
          + "(>= 1)"
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
        OptionSpec.u64("node-size",
          "iso only (ignored otherwise): bytes per graph node array (>= 1)"
          where default' = 64)
        OptionSpec.u64("node-depth",
          "iso only (ignored otherwise): graph nesting levels (>= 1)"
          where default' = 2)
        OptionSpec.u64("node-breadth",
          "iso only (ignored otherwise): child nodes per graph node"
          where default' = 1)
        OptionSpec.u64("chains",
          "mesh/cyclic/iso/actorref only (ignored for backpressure): chains to "
          + "inject (>= 1)"
          where default' = 8)
        OptionSpec.u64("ttl",
          "mesh/cyclic/iso/actorref only (ignored for backpressure): hops per "
          + "chain"
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
    let node_size = cmd.option("node-size").u64().usize()
    let node_depth = cmd.option("node-depth").u64().usize()
    let node_breadth = cmd.option("node-breadth").u64().usize()

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

    // chains is validated only where it is used (the chain-based workloads:
    // mesh/cyclic/iso/actorref); backpressure has no chains, so it carries none.
    let workload: (_Mesh | _Cyclic | _Backpressure | _Iso | _ActorRef) =
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
      elseif workload_name == "iso" then
        if pingers < 1 then return "--pingers must be >= 1" end
        if chains < 1 then return "--chains must be >= 1" end
        if node_size < 1 then return "--node-size must be >= 1" end
        if node_depth < 1 then return "--node-depth must be >= 1" end
        _Iso(pingers, chains, ttl, node_size, node_depth, node_breadth)
      elseif workload_name == "actorref" then
        if pingers < 1 then return "--pingers must be >= 1" end
        if chains < 1 then return "--chains must be >= 1" end
        _ActorRef(pingers, chains, ttl)
      else
        return "bad --workload (expected 'mesh', 'cyclic', 'backpressure', 'iso' "
          + "or 'actorref')"
      end

    _Config(seed, payload_string, payload_size, payload_fresh, workload)

primitive _Fatal
  """
  The harness's runtime-fault detector: print a diagnostic to stderr and exit
  non-zero. Used by the late/duplicate-message oracles across all workloads -- a
  ping, `iso` handoff, or `actorref` cite arriving after its node has reported, a
  mesh, cyclic, `iso`, or `actorref` completion beyond the expected total, or a
  backpressure `work`/`finished` after the `Consumer` is done (or more `finished`
  reports than producers) -- and by the `iso` workload's parcel-integrity oracle (a
  sentinel-byte mismatch in a delivered graph). Each is a real runtime fault -- a
  duplicated or delayed message, or a silently corrupted one -- so the stress run
  fails loudly with a location rather than passing.
  """
  fun apply(msg: String, loc: SourceLoc = __loc) =>
    let line = "FATAL: " + msg + " (" + loc.file() + ":"
      + loc.line().string() + ")\n"
    @fprintf(@pony_os_stderr(), "%s".cstring(), line.cstring())
    @exit(I32(1))

primitive _Unreachable
  """
  A branch the compiler can't prove is dead but we know is: every guarded array
  access here is in range by construction -- a neighbor or injection-start index
  from `Rand.int(size)` with `size >= 1`, or an `iso`-parcel array index guarded by
  the `node_size >= 1` invariant. If one ever fires it is a real bug, so crash with
  a location rather than continue with corrupt state (the "unreachable try/else"
  rule). Distinct from `_Fatal`: this flags a fault that should be impossible, not
  one the harness is built to catch.
  """
  fun apply(msg: String, loc: SourceLoc = __loc) =>
    let line = "UNREACHABLE: " + msg + " (" + loc.file() + ":"
      + loc.line().string() + ")\n"
    @fprintf(@pony_os_stderr(), "%s".cstring(), line.cstring())
    @exit(I32(1))
