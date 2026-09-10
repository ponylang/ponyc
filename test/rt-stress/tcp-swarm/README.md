# TCP swarm stress engine

A closed, count-driven TCP workload for stressing the net package's TCP stack. A fixed number
of client connections is churned through a listener at a bounded concurrency; each
client sends a stamped payload, the server echoes it, and the client verifies the
echo byte-for-byte before closing. Every behaviour is a CLI flag; the engine
(`tcp_swarm.pony`) draws nothing and sets no runtime defaults, and a
swarm orchestrator (`orchestrate_tcp.py`) draws the flags.

This is a port of ponyc's `tcp-swarm` runtime stress test. The draw and the run
mechanism are carried over from there; the engine uses the net package's API.

## How it stresses the net package

Heavy connection churn and lots of actor creation/destruction, with a content
oracle on top. Each flag is tied to a distinct code path in `tcp_connection.pony`:

- `--payload-size` / `--messages` — how much each connection sends, and in how many
  `send()` calls (chatty vs single).
- `--write-shape` (`write` | `writev`) — a single-buffer `send(ByteSeq)` vs a
  vectored `send(ByteSeqIter)`.
- `--writev-chunks` (`N`, writev only) — how many buffers one vectored `send`
  splits its payload into. Above `RuntimeBackend.writev_max()` (IOV_MAX on POSIX, 1 on
  Windows) a single `send` queues more buffers than one `writev` syscall can carry,
  so `_send_pending_writes()` takes its multi-batch path — one `writev_max`-sized
  batch per pass.
- `--expect` (`0` = off, `N` = frame size) — fixed-size framed reads via
  `buffer_until(MakeBufferSize(N))` vs whole-buffer `Streaming` reads, on both
  endpoints.
- `--close` (`graceful` | `hard`) — a graceful `close()` (FIN, drains) vs a muted
  `close()`, which the net package routes to `hard_close()` (immediate teardown, no lingering
  drain). The client closes only after its whole echo is back, so the hard path
  drops no data here — it exercises the distinct teardown/unsubscribe code.
- `--read-buffer-size` — the per-connection read buffer size (a `ReadBufferSize`),
  which sets both the initial allocation and the shrink-back floor. Because it sets
  the two equal, this varies read chunking and the yield threshold but does not
  exercise the net package's dynamic buffer resize/shrink paths (those need a mid-run resize).
- `--yield-after-reading` — after this many received bytes an endpoint calls
  `YieldReading` to leave the read loop cooperatively; reading resumes on its own
  the next scheduler turn. Small values yield often, giving other actors a chance
  to run mid-transfer.
- `--connections` / `--concurrency` — total connections to churn, and the in-flight
  cap.
- `--host` / `--port` — where the listener binds (default `localhost` / ephemeral).
  `localhost`, not the literal `127.0.0.1`, sidesteps a macOS ephemeral-port wall.

This is a plaintext echo workload, so it does not exercise the net package's SSL/TLS paths, its
idle/connection/user timers, or its socket options — its
idle/connection/user timers, or its socket options. Those are candidates for future swarm dimensions.

## Backpressure handling

The net package's `send()` is fallible: under backpressure it returns `SendErrorNotWriteable`
and does not queue the data on the application's behalf. Over loopback with
multi-megabyte payloads that happens constantly, on both endpoints, so the engine
handles it explicitly. This is the main way the engine differs from an echo test
written against the standard-library `net` package (whose `write()` queues
unboundedly and never fails), and it means the engine also exercises the net package's
backpressure and `mute`/`unmute` paths.

- **Client** — a resumable send-pump. It hands the connection one message at a time
  while `is_writeable()` is true, and resumes from `_on_unthrottled` after
  backpressure clears. It closes only once it has read its whole echo back.
- **Echo server** — when it cannot echo a chunk it stashes that one chunk and
  `mute()`s (which stops further reads, so at most one chunk is ever held), then
  sends the stash and `unmute()`s from `_on_unthrottled`. It checks `is_writeable()`
  *before* `send()`: `send(consume data)` consumes the buffer even when it returns
  an error, so a "try, then stash on failure" would drop the chunk.

The client never stops reading for write backpressure (only the server ever mutes,
and only while one echo chunk is stashed), so the two sides can't deadlock: the
client keeps draining the server's echo, which clears the server's backpressure and
lets it send the stash and unmute.

## Oracles

- **Echo integrity** — each connection sends a per-connection pseudo-random byte
  stream (the byte at position `p` is the low 8 bits of a splitmix64 hash of the
  connection id and `p`) and verifies every echoed byte against it. Systematic
  corruption — a run of wrong bytes, a misrouted chunk, a byte from another
  connection — is caught near-certainly; because the values are 8-bit, a lone
  single-byte reorder or duplicate aliases ~1/256. A short echo is caught by the
  conservation tally, not the byte check. Every connection must verify: the client
  closes only after it has read its whole echo back, so even a hard close tears down
  an already-drained connection.
- **Conservation** — every spawned connection reaches a terminal state (closed or
  connect-failed); the `RESULT` line reports the tally.
- **Crash / assert** — debug build, asserts on.

On success (every connection verified) the engine prints `RESULT ...` then `PASS`
and returns, letting the program reach natural quiescence. Anything short of full
verification — a connect failure, a short echo, or a byte mismatch — prints `FAIL`
and exits non-zero.

## Building and running

Build the engine with the orchestrator's `--ponyc` option (which compiles
from source) or directly with `ponyc`:

```bash
ponyc -d -o build/debug test/rt-stress/tcp-swarm     # -> build/debug/tcp-swarm
```

Run the engine directly for a single workload:

```bash
build/debug/tcp-swarm --connections 1000 --concurrency 64 --payload-size 256 \
  --messages 4
```

On Linux under WSL, connecting to an *unoccupied* port can hang; here the listener
occupies the port before any client dials, so the default `localhost` is fine.

When hand-running with `--expect N`, `N` must not exceed `--read-buffer-size`, and
`payload-size * messages` must be a whole number of `N`-byte frames — otherwise the
trailing partial frame is never delivered and the client would hang. The engine rejects
an `--expect` that violates either. The orchestrator satisfies both by construction.

Every flag is checked against a schema and its valid range: an unknown, misspelled, or
malformed flag, or a value out of range, is reported with a message and a non-zero exit
rather than silently falling back to a default. `--help` lists the flags.

## Running the swarm

The orchestrator draws one workload per seed (a random subset of the features above
plus bucketed magnitudes and a thin runtime backdrop — scaling, ASIO pinning, and
the cycle detector on or off) and runs the prebuilt engine once per seed. It does
not compile; point `--binary` at the engine you built above.

```bash
python3 test/rt-stress/tcp-swarm/orchestrate_tcp.py \
  --binary build/debug/tcp-swarm --count 50 --out ~/tmp/tcp-swarm-out
```

The draw is stable per seed, so a failure replays its *workload* from its number
(the concurrency timing a stress test hunts is not reproducible). Selectors:

- `--count N` / `--start S` — run N seeds from S.
- `--seeds A,B,C` — run specific seeds.
- `--replay N` — reproduce seed N's workload (note: `--ponymaxthreads` is redrawn
  against the local core count, so the runtime backdrop can differ across hosts).
- `--budget-seconds N` — run seeds from `--start` until N seconds pass (a soak).
- `--max-connections N` — cap each seed's connection count (useful where opens are
  slow).
- `--lldb <path>` — run each seed under lldb so a crash leaves a backtrace.

On macOS the orchestrator draws each workload from a narrower profile automatically
(no flag), because macOS loopback shares a finite kernel buffer pool a wide workload
can exhaust — which stalls a send and hangs the run; see `WORKLOAD_PROFILES` (the
`macos` profile) in `orchestrate_tcp.py` for the lists and why.

A run is a failure only if it crashes, mismatches, or hangs — makes no progress for
`--no-progress-seconds` (the completed count stops rising). A failure writes
`bundle-<seed>.json` to `--out`. A healthy run is never failed for running long: one
still making progress at the `--timeout-seconds` backstop is reported `incomplete`,
not failed.

`orchestrate_tcp_test.py` covers the pure pieces of the orchestrator (the draw, the
memory budget, the watchdog, the run classifiers): `python3 orchestrate_tcp_test.py`.

## Memory and time bounds

- **Memory** — the orchestrator caps each run at 14 GiB of address space
  (`RLIMIT_AS`, Linux), and the draw is trimmed to keep well under that cap: the
  memory-driving levers (connections, concurrency, messages, payload, writev-chunks,
  read-buffer) are drawn against a shared budget in a per-seed random order, so the
  trimmed lever rotates and every lever still reaches large on some seeds. The cap is
  on *virtual* address space, but the budget estimates *live* bytes — on ponyc's `net`
  stack that measured ~4-7x under the pool allocator's virtual high-water mark, which grows
  as the run is CPU-starved (a draw the budget put at ~1.2 GiB peaked ~5 GiB virtual on fast
  cores but ~8.4 GiB on 2 cores, where it reproduces the CI OOM; ~120 MiB RSS throughout).
  Virtual is nearly free (RSS is the scarce resource, the runner has 16 GiB), so the cap is
  set high enough to clear that ~8.4 GiB worst case with margin. The budget's
  cost constants (`MEM_OBJ_BYTES`, `MEM_RB_FACTOR`) were calibrated against ponyc's `net`
  stack, not measured independently — the *shape* carries over but the constants are unconfirmed
  here, so the net package is at least as exposed; confirm with a raised-cap run before
  trusting the budget as a tight bound.
- **Time** — the per-run clamp bounds round-trips (`connections * messages`) and
  total bytes (`connections * messages * payload`), so an outsized draw is trimmed.
  Bytes are the heavier cost: each payload byte is generated by a per-position hash
  (the stream is unique and non-repeating, so it can't be bulk-copied), which is why
  the byte ceiling is the one the clamp defends.
