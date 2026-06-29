# Generative runtime stress harness

A generative + swarm message-passing stress test for the Pony runtime. Each
master seed draws a different workload and runtime configuration, so a single
failure replays from one number. The same engine runs in **two modes**:

- **Systematic** (`orchestrate_systematic.py`) — runs the engine under
  `use=systematic_testing`: serialized to one thread at a time, with a fixed
  `--ponysystematictestingseed` replaying one scheduler interleaving.
  Reproducible; explores interleavings deterministically; by construction cannot
  catch true-simultaneity bugs.
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
two closed, count-driven workloads (`--workload`):

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

The traced payload is a `val String` or a `U64` no-GC control (`--payload`),
either forwarded as one object down a chain or re-allocated at each hop
(`--payload-mode forward|fresh`) — the swarm draws both. On success the engine
prints its result and lets the program reach **natural quiescence** (no forced
exit), so the `cyclic` workload's garbage is actually collected and the runtime's
clean shutdown is itself exercised; only a conservation *failure* forces a
non-zero exit. The engine holds no configuration logic and no
`@runtime_override_defaults`; every knob is a CLI flag set by the orchestrator.

The systematic mode draws only the `mesh` workload today; the `cyclic` workload
runs under the normal mode, where real-parallel collection catches concurrent
collection races the serialized systematic mode can't. (Running `cyclic` under
systematic with the determinism oracle is a deferred follow-up — it reproduces
with the detector on post-#5569, but needs the systematic driver's forced
`--ponynoblock` relaxed.)

## Running it

### Systematic mode

Needs a **debug + systematic** ponyc:

```bash
make configure config=debug use=scheduler_scaling_pthreads,systematic_testing
make build config=debug
python3 test/rt-stress/generative/orchestrate_systematic.py \
  --ponyc build/debug-scheduler_scaling_pthreads-systematic_testing/ponyc \
  --use-flags scheduler_scaling_pthreads,systematic_testing \
  --count 50 --out ~/tmp/rt-stress-out
```

Build it with clang. gcc currently fails to compile the systematic-testing
runtime ([#5563](https://github.com/ponylang/ponyc/issues/5563)); pass
`CC=clang CXX=clang++` to both `make` commands if gcc is your default.

### Normal mode

Needs a **normal debug** ponyc (no `use=` flags), which builds everywhere
including Windows:

```bash
make configure config=debug
make build config=debug
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

- **Conservation** — for the `mesh` workload, messages sent == received == the
  expected total (an independent per-pinger tally); a mismatch is a lost or
  duplicated message. The `cyclic` workload's actors are garbage by quiescence and
  can't be polled, so its conservation is *completion-count only*: exactly
  `generations * chains` terminal completions must arrive — weaker than the mesh's
  tally. The engine checks this itself and exits non-zero on a detected failure, so
  it holds in **both** modes.
- **Late message** — for `mesh`, anything arriving after a pinger has reported is a
  leak; for `cyclic`, a completion beyond the expected total is a duplicate. Both
  trip the engine's fatal-fault path. (A *lost* `cyclic` message instead leaves the
  completion count short forever — caught by liveness, not here.)
- **Cycle-detector collection** (`cyclic` workload) — the groups are genuine
  garbage cycles, so the detector must reclaim them; natural quiescence makes that
  collection run on every config, so a collection-path crash regression is reliably
  caught. When the swarm draws a small `--ponycdinterval`, the detector sweeps
  during the run (not just at teardown) under the normal mode's real parallelism,
  so collection additionally races the live pinging. (Not yet verified: an exact
  spawned == finalized count — finalisers
  can't send messages and run partly at teardown, so an explicit count is deferred;
  a gross silent leak is backstopped by `RLIMIT_AS`.)
- **Crash / `pony_assert`** — debug build, asserts on; a failed assert prints its
  own backtrace to the captured output, and the normal mode additionally runs
  each seed under lldb so even a raw crash with no assert is captured with a
  `bt all` (the systematic mode reproduces from its seed, so it re-runs under a
  debugger locally instead).
- **Liveness** — the orchestrator's wall-clock watchdog; a hang is a failure
  (including a `cyclic` chain that never completes, or a runtime that fails to
  shut down cleanly once the engine quiesces).
- **Determinism** (systematic mode only) — every seed runs twice and the two
  runs' `ORDER_SIG` must match; a divergence is a real race. This needs the
  serialized, reproducible runtime, so it is the systematic driver's oracle
  alone; the normal mode is non-reproducible and runs each seed once. The
  systematic driver always passes `--ponynoblock`: the oracle holds only with the
  cycle detector off, which walks the same maps and is its own ordering source.
  The normal mode instead draws `--ponynoblock` as a swarm knob, so the cycle
  detector is stressed when it is absent.

(Systematic replay was once address-dependent — the runtime ordered some work by
actor pointer values, which ASLR randomizes per run.
[#5566](https://github.com/ponylang/ponyc/pull/5566) and
[#5570](https://github.com/ponylang/ponyc/pull/5570) made that map iteration
creation-order keyed instead, so replay is now layout-independent on every
platform and no ASLR wrapper is needed.)

Not yet checked: the payload's bytes at the end of a chain. A non-crashing trace
or GC corruption (wrong bytes, truncation) would pass every oracle above —
conservation counts messages and `ORDER_SIG` mixes ids and counts. A
payload-integrity check is deferred to a later round, alongside richer
allocation-diversity work, an exact spawned == finalized count for the `cyclic`
workload, and running `cyclic` under the systematic determinism oracle.

## Tests

Three self-contained test files (no pytest), run in CI via `lint-python.yml`:

- `stress_common_test.py` — the shared pure pieces (seed derivation, the workload
  draws including `draw_cyclic`, output parsing, command building).
- `orchestrate_systematic_test.py` — the systematic config draw (pinned golden;
  always the `mesh` workload, unchanged by the cyclic work).
- `orchestrate_normal_test.py` — the normal config draw (no systematic seed,
  `ponynoblock` as a swarm knob, both workload kinds drawn, the cyclic memory
  ceiling, pinned golden).

Run one directly:

```bash
python3 test/rt-stress/generative/stress_common_test.py
```
