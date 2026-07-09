# Generative runtime stress harness

A generative + swarm message-passing stress test for the Pony runtime. Each
master seed draws a different workload and runtime configuration, so a single
failure replays from one number. The same engine runs in **two modes**:

- **Systematic** (`orchestrate_systematic.py`) — runs the engine under
  `use=systematic_testing`: serialized to one thread at a time, with a fixed
  `--ponysystematictestingseed` replaying one scheduler interleaving.
  Reproducible; explores interleavings deterministically; by construction cannot
  catch true-simultaneity bugs. Each seed also runs under **lldb**: a crash
  reproduces from its seed, but an intermittent hang need not (the same seed can
  pass and hang on the same host), so a watchdog timeout captures an all-thread
  backtrace of the hung engine in the moment, and a crash gets its backtrace in
  the bundle without a local re-run.
- **Normal** (`orchestrate_normal.py`) — runs the engine under a normal,
  production-default runtime: real multi-threaded parallelism. Catches the
  true-simultaneity bugs (atomic tearing, memory ordering, lock-free contention)
  the systematic mode can't, at the cost of reproducibility — runs are not
  deterministic, so there is no determinism oracle. Because a crash here doesn't
  reproduce, each seed runs under **lldb** so the crash backtrace is captured in
  the moment — it's the only artifact you'll get for a raced memory-corruption
  crash.

Both modes share their mechanism (seed derivation, the workload draws, the run
watchdog, the failure bundle) in **`stress_common.py`**; what differs between
them lives in the two driver scripts, not behind a flag.

## The engine

**`main.pony`** — the workload engine, identical in both modes. It draws one of
five closed, count-driven workloads (`--workload`):

- **`mesh`** — a closed mesh of `Pinger` actors: a fixed number of chains are
  injected, each with a hop count (TTL); a pinger forwards exactly one ping per
  received ping until the TTL expires. The run terminates deterministically when
  every chain has, and `sent == received == chains * (ttl + 1)` is an exact
  conservation invariant verified by polling every pinger's tally.
- **`cyclic`** — successive generations of strongly-connected actor groups (each
  member holds the whole group's array). After a generation's chains drain, the
  group's external reference is dropped, so it becomes garbage only the cycle
  detector can reclaim — this is what exercises the detector's collection path,
  which the static mesh never does. The actors are garbage by quiescence and can't
  be polled, so conservation here is completion-count only: exactly
  `generations * chains` terminal completions must arrive. A lost forward leaves a
  chain that never completes, surfacing as the orchestrator's liveness timeout; a
  duplicate/late completion trips the engine's fatal-fault path. (Because the oracle
  is a pure count, a lost forward and a duplicate in the same run can net out to the
  expected total and slip through — the mesh's independent tally would catch that;
  this is the deliberate narrowing of the cyclic oracle.) This is the proven shape
  of `test/rt-systematic/cycle-collection-order-signature`, parameterized for the
  swarm.
- **`backpressure`** — a star of `Producer` actors flooding one `Consumer`. The
  consumer explicitly participates in runtime backpressure via the stdlib
  `backpressure` package: after every `apply_every` received messages it calls
  `Backpressure.apply` (marking itself UNDER_PRESSURE, so its producers are muted)
  and self-sends a `lift` that calls `Backpressure.release`. The self-send is never
  muted, so apply is always paired with release and the closed flood cannot
  deadlock. This drives the explicit UNDER_PRESSURE muting path that the `mesh`
  never reaches — the mesh drives only the runtime's *automatic* OVERLOAD muting
  (which the swarm already exercises incidentally at high message volume). Each
  producer sends `messages` work messages then a `finished` report; completion
  triggers on `finished_count == producers` (independent of the received count) and
  then asserts `received == sent == producers * messages` from the producers' own
  send tallies — so conservation here is the mesh's strength, not the cyclic's
  weakness: a lost work is caught as a conservation failure, not merely a timeout.
- **`iso`** — the `mesh` topology, but the shared `val` payload is replaced by a
  freshly built nested `iso` object graph — a `Parcel` tree of `--node-depth`
  levels with `--node-breadth` children per node, every node separately allocated
  and holding its own byte array — that is `consume`d hop-to-hop. Each hop moves
  ownership of the whole mutable subgraph to the next actor, so the receiver
  acquires a foreign *mutable* multi-object graph node by node — the ORCA trace path
  no other workload reaches (the others only ever make a raw byte buffer mutable,
  never a typed object struct, and never transfer a mutable subgraph). The swarm
  draws the graph shape, so runs span a single flat node through a 15-node tree. At
  the terminal hop the graph is consumed into a `ref` and every node's sentinel
  bytes are verified, so a silent GC corruption or premature free of the moved
  subgraph is caught, not just a crash. Conservation is the mesh's full send/receive
  tally (`sent == received == chains * (ttl + 1)`); the `Carrier`s are live at
  quiescence (the `Dispatcher` holds them), so they are polled exactly like the
  mesh.
- **`actorref`** — the `mesh` topology, but the cargo is a `Referent tag` (a
  reference to a bare actor) instead of a `val` payload. A FRESH `Referent` is built
  per injected chain and its tag rides the chain hop-to-hop; each `Relay` forwards
  the tag onward and never stores it. A freshly received foreign actor reference sits
  at reference count 1, so forwarding it exhausts its weighted budget on that send —
  the forward drives ORCA's actor-reference ACQUIRE, and dropping it (never stored)
  drives the RELEASE. So `send_remote_actor` / `recv_remote_actor` / `acquire_actor`
  fire proportional to `chains * ttl` — the actor-reference machinery no other
  workload exercises under load (measured: the others hit `acquire_actor` 0–1 times a
  run, all at setup). A fixed referent pool was rejected by measurement: a shared
  referent's weighted budget stays high, so forwards stop acquiring and the path goes
  cold; fresh-per-chain also makes a lost release leak in proportion to `chains`.
  Conservation is the mesh's full send/receive tally
  (`sent == received == chains * (ttl + 1)`); the `Relay`s are live at quiescence
  (the `Referrer` holds them), so they are polled exactly like the mesh, while the
  referents become garbage as their chains drain (a premature free → a relay forwards
  a dangling tag → crash; a lost release → a leak).

For the mesh/cyclic/backpressure workloads the traced payload is a `val String` or
a `U64` no-GC control (`--payload`), either reused as one object (forwarded down a
chain, or re-sent by a producer) or re-allocated at each send
(`--payload-mode forward|fresh`) — the swarm draws both. The `iso` and `actorref`
workloads do not draw the val payload at all: `iso` has its OWN cargo knobs
(`--node-size`/`--node-depth`/`--node-breadth` shape the iso graph) and is
single-owner, so it can't ride the shared `(String val | U64)` payload union;
`actorref`'s cargo is a fresh actor `tag` built per chain in the engine, not a
payload value. On success the engine prints its result and lets the
program reach **natural
quiescence** (no forced exit), so the `cyclic` workload's garbage is actually
collected and the runtime's clean shutdown is itself exercised; only a
conservation *failure* forces a non-zero exit. The engine holds no configuration
logic and no `@runtime_override_defaults`; every knob is a CLI flag set by the
orchestrator.

The systematic mode draws all five workloads (`mesh`, `cyclic`, `backpressure`,
`iso`, `actorref`), with `--ponynoblock` drawn ~50% so the cycle detector runs under
the reproducible oracle — cyclic collection and iso acquire both reproduce with the
detector on, because the detector's recipient-scheduling sends are sorted by a stable
actor id, so replay is layout-independent. `backpressure` is cycle-detector-independent;
its determinism instead rides on the muting/unmuting arrival order, which also
reproduces under replay — its per-work `ORDER_SIG` fold fingerprints that interleaving
(see main.pony and .known-couplings/backpressure-workload-muting.md). `actorref` rides
on the same routing interleaving as mesh/iso (its per-completion `ORDER_SIG` fold), and
it is the heaviest exerciser of the id-sorted actor acquire/release drain — a regression
in that sort would perturb its `ORDER_SIG` (see
.known-couplings/systematic-testing-send-ordering.md). Systematic keeps
cyclic/backpressure/iso/actorref small (see the `SYSTEMATIC_*` caps in
stress_common.py) so the serialized soak's per-seed cost stays at or below the
mesh-only cost it replaces; the normal mode still carries the large-magnitude
cyclic/iso/actorref runs, where real parallelism also catches the concurrent-collection,
mute/unmute, mutable-subgraph acquire, and actor-reference acquire/release races the
serialized mode can't.

## Running it

### Systematic mode

Needs a **debug + systematic** ponyc:

```bash
cmake --preset debug -DPONY_USES=scheduler_scaling_pthreads,systematic_testing
cmake --build --preset debug
python3 test/rt-stress/generative/orchestrate_systematic.py \
  --ponyc build/debug-scheduler_scaling_pthreads-systematic_testing/ponyc \
  --use-flags scheduler_scaling_pthreads,systematic_testing \
  --count 50 --out ~/tmp/rt-stress-out
```

Build it with clang. gcc currently fails to compile the systematic-testing
runtime ([#5563](https://github.com/ponylang/ponyc/issues/5563)); the `debug`
preset uses clang regardless of your default compiler.

Each seed runs under `lldb` (on `PATH` by default; `--lldb PATH` for a custom
location), so a crash or watchdog timeout leaves a backtrace in the failure
bundle. `--no-lldb` runs the engine directly with no backtrace capture — for
hosts without lldb; no CI lane passes it. Running under lldb does not change the
interleaving — the determinism oracle holds under it, on every TIER1 platform.

### Normal mode

Needs a **normal debug** ponyc (no `use=` flags), which builds everywhere
including Windows:

```bash
cmake --preset debug
cmake --build --preset debug
python3 test/rt-stress/generative/orchestrate_normal.py \
  --ponyc build/debug/ponyc --budget-seconds 1800 --out ~/tmp/rt-stress-out
```

Each seed runs under `lldb` (on `PATH` by default; `--lldb PATH` for a custom
location), so lldb must be installed. `--budget-seconds N` runs seeds from
`--start` incrementing until N wall-clock seconds elapse (the CI soak). Both modes
also take `--count N`, `--master-seed N`, `--seeds A,B,C`, and `--replay N`
(reconstruct one run's config and CLI — under normal mode the run is
non-deterministic, so a replay won't necessarily reproduce a failure).

## Oracles

- **Conservation** — the `mesh`, `iso`, and `actorref` workloads poll an exact,
  independent per-actor send/receive tally from the live pingers/carriers/relays
  (`sent == received == chains * (ttl + 1)`); a duplicate or miscounted message shows
  as a tally mismatch. (For `actorref` the tally is on the live relays; the referents
  are garbage by quiescence but carry no tally — they are pure cargo.) A *lost*
  mesh/iso/actorref message does NOT show here — the chain never completes, so the
  tally is never polled — it surfaces as a liveness timeout instead. The
  `backpressure` workload is the strongest on *lost* messages: its `finished`
  reports are sent independently of work delivery, so a lost work still lets
  `_finish` run and assert received == the producers' send tally == the expected
  total — a conservation failure, not just a timeout. The `cyclic` workload's actors
  are garbage by quiescence and can't be polled, so its conservation is
  *completion-count only* (exactly `generations * chains` completions) — weaker than
  the `mesh`/`iso` exact tally. The engine checks this itself and exits non-zero on a
  detected failure, so it holds in **both** modes.
- **Late / duplicate message** — for `mesh`, a ping arriving after a pinger has
  reported is a leak, or a completion beyond `chains` is a duplicate; for
  `backpressure`, a work or `finished` after completion, or more `finished`
  reports than producers, is a duplicate; for `cyclic`, a completion beyond the
  expected total is a duplicate; for `iso`, a handoff arriving after a `Carrier`
  has reported, or a completion beyond `chains`, is a duplicate; for `actorref`, a
  `cite` arriving after a `Relay` has reported, or a completion beyond `chains`, is a
  duplicate. All trip the engine's fatal-fault path. (A *lost* mesh/cyclic/iso/actorref
  message instead leaves its chain incomplete forever — caught by the liveness
  timeout, not here; only `backpressure` catches a lost message as a conservation
  failure.)
- **Parcel integrity** (`iso` workload) — the only oracle in the harness that
  checks message *contents*, not just counts. Every node of the graph is built with
  sentinel bytes; at the terminal hop the `Carrier` consumes the graph into a `ref`
  and walks the whole tree, checking the first and last byte of each node's array,
  so a silent GC corruption or premature free that disturbs an endpoint sentinel —
  which the message-counting oracles cannot see — trips the fatal-fault path. This
  is endpoint sampling, not a full scan: it *narrows* the payload-integrity gap
  (below) for `iso` rather than closing it (interior-only corruption, or a premature
  free whose memory is not yet reused, still slips through); the other three
  workloads check no bytes at all.
- **Backpressure / muting** (`backpressure` workload) — the consumer explicitly
  applies and releases runtime backpressure, driving the UNDER_PRESSURE mute/unmute
  reschedule path (the stdlib `backpressure` FFI, the `triggers_muting` /
  global-unmute machinery) that nothing else in the harness reaches; the mesh only
  ever triggers *automatic* OVERLOAD muting. Under the normal mode's real
  parallelism the mute/unmute races the live flood. (Muting actually firing is a
  calibrated property of the drawn shape — like `test/rt-systematic/`
  `mute-order-signature`, the per-run oracles are conservation/crash/liveness, not a
  muting assertion.)
- **Cycle-detector collection** (`cyclic` workload) — the groups are genuine
  garbage cycles, so the detector must reclaim them; natural quiescence makes that
  collection run on every config, so a collection-path crash regression is reliably
  caught. When the swarm draws a small `--ponycdinterval`, the detector sweeps
  during the run (not just at teardown) under the normal mode's real parallelism,
  so collection additionally races the live pinging. (Not yet verified: an exact
  spawned == finalized count — finalisers
  can't send messages and run partly at teardown, so an explicit count is deferred;
  a gross silent leak is backstopped by `RLIMIT_AS`.)
- **Actor-reference machinery** (`actorref` workload) — forwarding a fresh, unheld
  actor `tag` drives ORCA's actor-reference send/recv/acquire/release proportional to
  `chains * ttl` (the path the other workloads leave cold). This is *coverage*: a bug
  in that machinery surfaces as a crash or a debug `pony_assert` under the heavy churn
  (an over-count → a premature free of a referent → a relay forwards a dangling tag),
  or as memory growth past the calibrated bound (an under-count → a lost release →
  referents leak, and fresh-per-chain makes that leak scale with `chains`). Honest
  boundary: there is no in-Pony referent tally (a `tag` can't observe its own ORCA
  receives), so a *small* under-count that doesn't push memory over the bound still
  slips — the same class of gap `iso` documents for interior corruption. Under the
  normal mode's real parallelism the acquire/release races the live forwarding.
- **Crash / `pony_assert`** — debug build, asserts on; a failed assert prints its
  own backtrace to the captured output, and both modes run each seed under lldb
  so even a raw crash with no assert is captured with a `bt all` in the bundle.
  On a watchdog timeout the orchestrator stops the hung engine under lldb first
  — SIGABRT on POSIX, an injected breakpoint on Windows — so a hang is captured
  with the same all-thread backtrace a crash gets, on every lane. Reading a
  Windows timeout bundle: the stopped thread is the one the breakpoint was
  injected into (`ntdll!DbgBreakPoint`), not a thread that was doing anything.
  It is an artifact of the capture. The hung threads are the other entries in
  `bt all`, and `frame variable` describes the injected thread, so it says
  nothing about the engine.
- **Liveness** — the orchestrator's watchdog; a hang is a failure (including a
  `cyclic` chain that never completes, or a runtime that fails to shut down
  cleanly once the engine quiesces). In **normal** mode the watchdog fires on **no
  progress** — the engine emits a flushed heartbeat as it advances, and a run that
  stays silent too long is hung — so a slow-but-progressing run finishes rather
  than being false-failed for taking a long time. **Systematic** mode keeps a flat
  total-time watchdog (its runs are short and reproducible, and its park-site hangs
  must still surface as a timeout).
- **Determinism** (systematic mode only) — every seed runs twice and the two
  runs' `ORDER_SIG` must match; a divergence is a real race. This needs the
  serialized, reproducible runtime, so it is the systematic driver's oracle
  alone; the normal mode is non-reproducible and runs each seed once. Both drivers
  draw `--ponynoblock` as a swarm knob (~50%): the oracle holds with the cycle
  detector on as well as off (the detector's recipient-scheduling sends are id-sorted,
  so replay is layout-independent), so a systematic run stresses the detector's
  collection under the reproducible oracle when the knob is absent.

Not yet checked: the payload's bytes for the mesh/cyclic/backpressure workloads. A
non-crashing trace or GC corruption (wrong bytes, truncation) on one of those would
pass every oracle above — conservation counts messages and `ORDER_SIG` mixes ids
and counts. (The `iso` workload *narrows* this for itself — its terminal `Carrier`
checks the endpoint sentinel bytes of the parcel; see Parcel integrity above — but
interior corruption still slips through there too, and the other workloads check no
bytes.)

## Tests

Three self-contained test files (no pytest), run in CI via `lint-python.yml` — on
Linux and on Windows, since the timeout capture's real-lldb integration test has a
distinct mechanism on each and would otherwise go unexercised on one of them:

- `stress_common_test.py` — the shared pure pieces (seed derivation, the workload
  draws including `draw_workload`, output parsing, command building).
- `orchestrate_systematic_test.py` — the systematic config draw (pinned per-kind
  goldens; draws `mesh`/`cyclic`/`backpressure`/`iso`/`actorref` with `--ponynoblock`
  as a swarm knob).
- `orchestrate_normal_test.py` — the normal config draw (no systematic seed,
  `ponynoblock` as a swarm knob, all five workload kinds drawn, the cyclic memory
  ceiling, the backpressure message ceiling, the iso chains/ttl burst ceilings, the
  actorref chains ceiling, pinned per-kind goldens).

Run one directly:

```bash
python3 test/rt-stress/generative/stress_common_test.py
```
