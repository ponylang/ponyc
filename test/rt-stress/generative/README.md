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

**`main.pony`** — the workload engine, identical in both modes. A closed mesh of
`Pinger` actors: a fixed number of chains are injected, each with a hop count
(TTL); a pinger forwards exactly one ping per received ping until the TTL
expires. The run terminates deterministically when every chain has, and `sent ==
received == chains * (ttl + 1)` is an exact conservation invariant the engine
checks itself, exiting non-zero on a mismatch. The traced payload is a `val
String` or a `U64` no-GC control (`--payload`), either forwarded as one object
down a chain or re-allocated at each hop (`--payload-mode forward|fresh`) — the
swarm draws both. It holds no configuration logic and no
`@runtime_override_defaults`; every knob is a CLI flag set by the orchestrator.

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

- **Conservation** — messages sent == received == the expected total; a mismatch
  is a lost or duplicated message. The engine checks this itself and exits
  non-zero on failure, so it holds in **both** modes.
- **Late message** — anything arriving after a pinger has reported is a leak.
- **Crash / `pony_assert`** — debug build, asserts on; a failed assert prints its
  own backtrace to the captured output, and the normal mode additionally runs
  each seed under lldb so even a raw crash with no assert is captured with a
  `bt all` (the systematic mode reproduces from its seed, so it re-runs under a
  debugger locally instead).
- **Liveness** — the orchestrator's wall-clock watchdog; a hang is a failure.
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
conservation counts messages, `ORDER_SIG` mixes ids and counts, and the
late-message check keys on a flag. A payload-integrity check is deferred to the
next round, alongside the richer allocation-diversity work.

## Tests

Three self-contained test files (no pytest), run in CI via `lint-python.yml`:

- `stress_common_test.py` — the shared pure pieces (seed derivation, the workload
  draws, output parsing, command building).
- `orchestrate_systematic_test.py` — the systematic config draw (pinned golden,
  byte-identical to the pre-split mapping so historical seeds replay).
- `orchestrate_normal_test.py` — the normal config draw (no systematic seed,
  `ponynoblock` as a swarm knob, pinned golden).

Run one directly:

```bash
python3 test/rt-stress/generative/stress_common_test.py
```
