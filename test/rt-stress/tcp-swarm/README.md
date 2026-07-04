# Swarm TCP stress engine

A closed, count-driven TCP workload for stressing the runtime's net stack. Every
behaviour is a CLI flag; the engine (`main.pony`) draws nothing and sets no runtime
defaults, and a swarm orchestrator (`orchestrate_tcp.py`) draws the flags. A fixed
number of client connections is churned through a listener at a bounded
concurrency; each client sends a stamped payload, the server echoes it, and the
client verifies the echo byte-for-byte before closing.

## How it stresses the runtime

Heavy ASIO activity and lots of actor creation/destruction, with a content oracle
on top and swarm dimensions each tied to a distinct code path in
`packages/net/tcp_connection.pony`:

- `--payload-size` / `--messages` — how much each connection sends, and in how many
  messages (chatty vs single).
- `--write-shape` (`write` | `writev`) — single vs vectored writes.
- `--writev-chunks` (`N`, writev only) — how many buffers a `writev` splits its
  payload into. Above `@pony_os_writev_max()` (IOV_MAX — 1024 on Linux/macOS — on
  POSIX, 1 on Windows) a single writev queues more buffers than one syscall sends,
  so
  `TCPConnection` takes its multi-batch send path. That path is the only one that
  re-checks `--yield-after-writing`, so on POSIX the mid-write yield needs a
  chunk count above IOV_MAX to fire at all.
- `--expect` (`0` = off, `N` = frame size) — fixed-size framed reads vs
  whole-buffer, on both endpoints.
- `--close` (`graceful` | `hard`) — a graceful `dispose()` (FIN, drains) vs a muted
  `dispose()`, which takes the `hard_close` path (immediate teardown, no lingering
  drain). The client closes only after its whole echo is back, so the hard path
  drops no data here — it exercises the distinct teardown/unsubscribe code.
- `--read-buffer-size` / `--yield-after-reading` / `--yield-after-writing` — the
  TCPConnection read-buffer size and the byte counts at which it yields back to the
  scheduler mid-read/mid-write. A small `--yield-after-reading` yields on any
  payload that fills the read buffer more than once; `--yield-after-writing` only
  bites on the multi-batch write path (see `--writev-chunks`), so on POSIX it does
  nothing unless the chunk count is above IOV_MAX.
- `--connections` / `--concurrency` — total connections to churn, and the in-flight
  cap.
- `--host` / `--port` — where the listener binds (default `localhost` / ephemeral).
  `localhost`, not the literal `127.0.0.1`, sidesteps a macOS ephemeral-port wall
  (see the note in `main.pony`).

## Oracles

- **Echo integrity** — each connection sends a unique, non-repeating byte stream
  (byte at stream position `p` is a splitmix64 hash of the connection id and `p`)
  and verifies every echoed byte against it. Because the stream is unique per
  connection and never repeats, this catches not only a corrupted byte or a short
  read but a byte delivered out of order, duplicated, or from another connection.
  Every connection must verify: the client closes only after it has read its whole
  echo back, so even a hard close tears down an already-drained connection.
- **Conservation** — every spawned connection reaches a terminal state (closed or
  connect-failed); the `RESULT` line reports the tally.
- **Crash / assert** — debug build, asserts on.

This is a stress test, not fault injection: every connection is expected to
connect, exchange, and verify. On success (every connection verified) it prints
`RESULT ...` then `PASS` and returns, letting the program reach natural
quiescence. Anything short of full verification — a connect failure, a short echo,
or a byte mismatch — prints `FAIL` and exits non-zero.

## Running the swarm

Build a debug ponyc, then let the orchestrator compile the engine and run seeds.
Each seed draws one workload (a random subset of the features above plus bucketed
magnitudes and a thin runtime backdrop — scaling, ASIO pinning, and the cycle
detector on or off); the draw is stable per seed, so a failure replays from its
number.

```bash
make configure config=debug
make build config=debug
python3 test/rt-stress/tcp-swarm/orchestrate_tcp.py \
  --ponyc build/debug/ponyc --count 50 --out ~/tmp/tcp-swarm-out
```

`--seeds A,B,C` runs specific seeds; `--budget-seconds N` runs seeds from `--start`
until N seconds pass (the soak); `--lldb <path>` runs each seed under lldb so a
crash leaves a backtrace; `--max-connections N`, `--max-exchanges N`, and
`--max-bytes N` lower the per-run ceilings on connection count, round-trips, and
bytes moved (Windows CI uses all three — it opens sockets slowly *and* moves bytes
slowly; see "Memory and time bounds"). A non-passing run writes
`bundle-<seed>.json` to `--out`.

## Running the engine directly

```bash
cd build/debug
PONYPATH=../../packages ./ponyc -d -b tcp_swarm ../../test/rt-stress/tcp-swarm
./tcp_swarm --connections 1000 --concurrency 64 --payload-size 256 --messages 4
```

On Linux under WSL, connecting to an *unoccupied* port can hang; here the listener
occupies the port before any client dials, so the default `localhost` is fine.

## Memory and time bounds

- **Memory** — the orchestrator caps each run at 8 GiB of address space
  (`RLIMIT_AS`), and the *draw itself* is bounded so no config can reach that cap. A
  config that did would be killed by the runtime's own out-of-memory abort — which
  reads like a runtime crash but is really the draw asking for more memory than the
  cap allows, a false failure. So the memory-driving levers (connections, concurrency,
  messages, payload, writev-chunks, read-buffer) are drawn against a shared memory
  budget (`MEM_BUDGET_BYTES`, 2 GiB of workload under the cap): the draw spends the
  budget, and once it is spent the remaining levers are trimmed to fit. The levers are
  drawn in a per-seed *random order*, so the trimmed lever rotates — on one seed a big
  `--writev-chunks` squeezes concurrency and messages, on another a big connection
  count squeezes `--writev-chunks` — and every lever still reaches large on some
  seeds, keeping the swarm a swarm. Why 8 GiB and not 4: the Pony runtime reserves a
  flat ~3.5 GiB of *virtual* address space regardless of config and `RLIMIT_AS` caps
  virtual, so a 4 GiB cap sat ~86% full at baseline and risked a false OOM on a
  high-thread run. The budget's cost constants (used by `est_peak_bytes`) are
  calibrated from local measurement with a margin and are first-CI-run calibration
  items; see `.known-couplings/tcp-swarm-memory-budget.md`.
- **Time** — the per-run clamp (`clamp_run`) bounds round-trips
  (`connections * messages`) and total bytes (`connections * messages * payload`),
  so an outsized draw (e.g. 100k conns × 64 msgs × 64 KiB ≈ 400 GB) is trimmed.
  Bytes are the heavier cost: each payload byte is generated by a per-position hash
  (the stream is unique and non-repeating, so it can't be bulk-copied), which is why
  the byte ceiling — not just the round-trip ceiling — is the one the clamp defends.
  The `RUN_MAX_EXCHANGES` and `RUN_MAX_BYTES` defaults are sized for Linux
  throughput (best guesses, still to be tuned from CI); both are overridable via
  `--max-exchanges`/`--max-bytes` (like `--max-connections`), which Windows CI
  lowers because its loopback moves bytes far more slowly — an unclamped heavy
  draw can't finish inside the no-progress window there. Each run's wall-clock
  time is reported as `elapsed=Ns` in its summary line, the raw signal for tuning
  these ceilings from CI.
