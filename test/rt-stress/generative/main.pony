"""
Generative runtime stress harness (M0 -- plumbing).

A closed, count-driven mesh of `Pinger` actors that exercises the Pony runtime's
message passing and -- for the `String` payload kind -- ORCA tracing, with a
*provable* conservation oracle.

Unlike `string-message-ubench` (an open cascade stopped by a wall-clock timer),
this workload is closed: a fixed number of chains are injected, each carrying a
hop counter (TTL). A `Pinger` receiving a ping with hops > 0 forwards exactly
one ping (hops - 1) to a random neighbor; a ping with hops == 0 terminates and
signals the coordinator. Because each ping produces exactly one successor
message, the run terminates deterministically once every chain has terminated
-- no timer is needed (time is not virtualized under systematic testing) -- and

    sent == received == chains * (ttl + 1)

is an exact, checkable invariant.

The closed shape is a deliberate constraint, not the end goal. A continuous,
open workload -- always running, never quiescing -- is the better stress model:
continuous beats epochal, keeping the runtime under sustained, varied load
rather than in repeated run-to-quiescence epochs. We use a closed workload here
because it is what's feasible under systematic testing today: it yields the
exact conservation invariant above, and it needs no timer (time is not
virtualized under systematic testing, so a timer-driven open cascade like
ubench's would not reproduce). When open becomes feasible -- virtualized logical
time under systematic testing, or a non-systematic continuous mode -- revisit
this and switch.

All randomness is seeded only from `--seed` (never the clock). The routing graph
is not a function of `--seed` alone, though: which draw lands on which ping
depends on the order a Pinger processes its mailbox, so the routing — and the
run — reproduce only when the interleaving does (a fixed
`--ponysystematictestingseed` AND ASLR disabled; the orchestrator disables it).
The coordinator folds chain-completion arrivals into an FNV-1a `ORDER_SIG` -- a
pure function of the scheduler interleaving -- as the observable for the
determinism oracle.

This program holds no `@runtime_override_defaults`: every runtime knob (scaling,
cycle detector, GC, thread count, the systematic seed) is a `--pony*` flag set
by the orchestrator, and every workload parameter is a `--flag value` arg parsed
here. It is driven by `orchestrate.py` in this directory.
"""
use "cli"
use "random"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stderr[Pointer[U8]]()
use @exit[None](status: I32)

type Payload is (String val | U64)
  """
  A ping's traced cargo: a heap-allocated `String val` (gives ORCA something to
  trace across the actor boundary) or a `U64` primitive (a no-GC control). The
  kind is fixed for a whole run by `--payload`; `--payload-mode` then decides
  whether one payload object is forwarded the whole chain or a fresh one is
  allocated at each hop.
  """

primitive _Payloads
  """
  Builds a ping payload of the configured kind. `--payload-mode forward` reuses
  one object down a chain (exercising ORCA's shared-`val` refcounting); `fresh`
  calls this at every hop (exercising allocation + tracing + collection churn).
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
    Parse the workload args with the `cli` package and hand a validated config to
    the coordinator. `--help` prints usage and exits 0; a syntax error (unknown
    flag, missing value, bad number) or a failed value check exits non-zero
    without starting the workload.
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
      | let config: _Config => Coordinator(config)
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
  Builds the mesh, injects the chains, counts completions to detect quiescence,
  collects per-`Pinger` send/receive tallies, and evaluates the conservation
  oracle. On completion it prints `ORDER_SIG` plus the tallies and forces exit.
  """
  let _config: _Config
  let _pingers: Array[Pinger] val
  var _completions: USize = 0
  var _reports_outstanding: USize = 0
  var _total_sent: U64 = 0
  var _total_received: U64 = 0
  var _sig: U64 = 14695981039346656037 // FNV-1a 64-bit offset basis

  new create(config: _Config) =>
    _config = config

    // Create the mesh. Pingers are built outside the `recover` block (so we can
    // pass `this`) and pushed into the iso array via automatic receiver
    // recovery -- `push`'s argument, a `Pinger tag`, is sendable.
    let ps: Array[Pinger] iso = recover Array[Pinger](config.pingers) end
    var id: USize = 0
    while id < config.pingers do
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
    while chain < config.chains do
      let start = r.int(pingers.size().u64()).usize()
      try
        pingers(start)?.ping(_Payloads.make(config), config.ttl, chain)
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
    if _completions == _config.chains then
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
    let total_sent = _total_sent + _config.chains.u64()
    let expected = _config.chains.u64() * (_config.ttl.u64() + 1)
    let conserved =
      (total_sent == _total_received) and (_total_received == expected)

    // Synchronous print + forced exit: `env.out.print` is async and would be
    // lost on `@exit`, and the forced exit sidesteps the shutdown-hang symptom
    // seen under systematic testing (see test/rt-systematic/order-signature).
    let line = "RECEIVED=" + _total_received.string()
      + " SENT=" + total_sent.string()
      + " EXPECTED=" + expected.string()
      + " ORDER_SIG=" + _sig.string() + "\n"
    @printf("%s".cstring(), line.cstring())

    if conserved then
      @exit(I32(0))
    else
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

class val _Config
  """
  A validated workload configuration. Built only by `_Cli.config`, so the rest
  of the program can trust its invariants (pingers >= 1, chains >= 1, and
  payload_size >= 1 when payload_string).
  """
  let seed: U64
  let pingers: USize
  let chains: USize
  let ttl: USize
  let payload_string: Bool
  let payload_size: USize
  let payload_fresh: Bool

  new val create(seed': U64, pingers': USize, chains': USize, ttl': USize,
    payload_string': Bool, payload_size': USize, payload_fresh': Bool)
  =>
    seed = seed'
    pingers = pingers'
    chains = chains'
    ttl = ttl'
    payload_string = payload_string'
    payload_size = payload_size'
    payload_fresh = payload_fresh'

primitive _Cli
  """
  Builds the workload command spec and extracts a validated `_Config` from a
  parsed command. The `cli` package handles unknown flags, missing values, bad
  numbers, and `--help`; this adds the value-set and lower-bound checks it
  can't express. Runtime `--pony*` flags are stripped by the runtime before
  `Main` sees `env.args`, so they never reach the parser.
  """
  fun spec(): CommandSpec ? =>
    CommandSpec.leaf("generative",
      "Generative runtime stress workload (driven by orchestrate.py)",
      [
        OptionSpec.u64("seed",
          "engine RNG seed"
          where default' = 1)
        OptionSpec.u64("pingers",
          "number of mesh actors (>= 1)"
          where default' = 8)
        OptionSpec.u64("chains",
          "chains to inject (>= 1)"
          where default' = 8)
        OptionSpec.u64("ttl",
          "hops per chain"
          where default' = 16)
        OptionSpec.string("payload",
          "traced payload kind: string | u64"
          where default' = "string")
        OptionSpec.u64("payload-size",
          "string payload size in bytes (>= 1)"
          where default' = 64)
        OptionSpec.string("payload-mode",
          "per-hop payload allocation: forward | fresh"
          where default' = "forward")
      ])?.>add_help()?

  fun config(cmd: Command): (_Config | String) =>
    let seed = cmd.option("seed").u64()
    let pingers = cmd.option("pingers").u64().usize()
    let chains = cmd.option("chains").u64().usize()
    let ttl = cmd.option("ttl").u64().usize()
    let payload_size = cmd.option("payload-size").u64().usize()
    let payload = cmd.option("payload").string()
    let mode = cmd.option("payload-mode").string()

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

    if pingers < 1 then return "--pingers must be >= 1" end
    if chains < 1 then return "--chains must be >= 1" end
    if payload_string and (payload_size < 1) then
      return "--payload-size must be >= 1 for string payload"
    end

    _Config(seed, pingers, chains, ttl, payload_string, payload_size,
      payload_fresh)

primitive _Fatal
  """
  The harness's runtime-fault detector: print a diagnostic to stderr and exit
  non-zero. Used by the late/leaked-message oracle -- a ping arriving after a
  `Pinger` has reported is a duplicated or delayed message, a real runtime fault,
  so the stress run fails loudly with a location rather than passing.
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
