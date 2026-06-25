# Generative runtime stress harness

A generative + swarm message-passing stress test for the Pony runtime. Where
`string-message-ubench` runs one fixed workload, this draws a different workload
and runtime configuration from each master seed, so a single failure replays
from one number.

Two pieces:

- **`orchestrate.py`** — the harness. Owns *how to run*: it derives two seeds
  from a master seed (the program's RNG and the scheduler interleaving), draws a
  swarm configuration (workload shape plus a random subset of the non-timing
  runtime knobs), compiles the engine once with a provided `ponyc`, runs it under
  a wall-clock watchdog and an address-space cap, decides pass/fail, and writes a
  failure bundle. It can loop over many seeds, replay one, or run the determinism
  oracle.
- **`main.pony`** — the workload engine. A closed mesh of `Pinger` actors: a
  fixed number of chains are injected, each with a hop count (TTL); a pinger
  forwards exactly one ping per received ping until the TTL expires. The run
  terminates deterministically when every chain has, and `sent == received ==
  chains * (ttl + 1)` is an exact conservation invariant. The traced payload is a
  `val String` or a `U64` no-GC control (`--payload`), either forwarded as one
  object down a chain or re-allocated at each hop (`--payload-mode forward|fresh`)
  — the swarm draws both. It holds no configuration logic and no
  `@runtime_override_defaults`; every knob is a CLI flag set by the harness.

## Running it

The harness needs a **debug + systematic** ponyc:

```bash
make configure config=debug use=scheduler_scaling_pthreads,systematic_testing
make build config=debug
```

Build it with clang. gcc currently fails to compile the systematic-testing
runtime ([#5563](https://github.com/ponylang/ponyc/issues/5563)); pass
`CC=clang CXX=clang++` to both `make` commands if gcc is your default.

Then drive it from a master seed (or a range):

```bash
python3 test/rt-stress/generative/orchestrate.py \
  --ponyc build/debug-scheduler_scaling_pthreads-systematic_testing/ponyc \
  --use-flags scheduler_scaling_pthreads,systematic_testing \
  --count 50 --out ~/tmp/rt-stress-out
```

Other seed selectors: `--master-seed N` (one seed), `--seeds A,B,C`, and
`--replay N` (reconstruct one run's exact config and CLI). Every seed runs twice
and its observable `ORDER_SIG` must match (the determinism check is always on; a
divergence is a race).

## Oracles

- **Conservation** — messages sent == received == the expected total; a mismatch
  is a lost or duplicated message.
- **Late message** — anything arriving after a pinger has reported is a leak.
- **Crash / `pony_assert`** — debug build, asserts on.
- **Liveness** — the harness's wall-clock watchdog; a hang is a failure.
- **Determinism** (always on) — every seed runs twice and the two runs'
  `ORDER_SIG` must match. The harness disables ASLR (`setarch -R` on Linux) for
  every run: systematic-testing replay is otherwise address-dependent (the runtime
  orders some work by actor pointer values, which ASLR randomizes per run), so
  without it this oracle would report false races. A runtime fix to make the
  systematic build layout-independent is in progress; once it lands the wrapper
  becomes a harmless no-op.

Not yet checked: the payload's bytes at the end of a chain. A non-crashing trace
or GC corruption (wrong bytes, truncation) would pass every oracle above —
conservation counts messages, `ORDER_SIG` mixes ids and counts, and the
late-message check keys on a flag. A payload-integrity check is deferred to the
next round, alongside the richer allocation-diversity work.

## Tests

`orchestrate_test.py` covers the harness's pure pieces (seed derivation, swarm
config, output parsing) and runs in CI via `lint-python.yml`. Run it directly:

```bash
python3 test/rt-stress/generative/orchestrate_test.py
```
