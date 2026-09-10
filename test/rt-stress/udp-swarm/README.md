# UDP flood stress engine

A count-driven, one-way UDP workload for stressing the net package's UDP stack. A fixed
number of clients send stamped datagrams to a server; the server verifies the
payload of each received datagram against a per-client keystream and reports
results via actor messaging. There is no UDP echo: the only UDP traffic is
client-to-server.

This stress test was written to empirically verify that the ASIO backend
delivers persistent edge-triggered notifications correctly for UDP sockets
under sustained load.

## How it stresses the net package

No connection lifecycle: UDP sockets bind, send, receive, and close. The point
is to exercise the readiness event delivery path under sustained datagram
volume, verifying that the ASIO backend correctly delivers persistent
edge-triggered notifications for UDP sockets across many read-loop re-entries.
Each flag is tied to a distinct code path in `udp_socket.pony`:

- `--datagrams` / `--payload-size` -- volume and per-datagram size.
- `--batch-size` -- how many datagrams a client sends per scheduling turn
  before yielding. The client sends continuously until all datagrams are sent.
- `--clients` -- concurrent client sockets sending to the same server.
- `--read-buffer-size` -- the per-socket read buffer, which sets the byte
  budget in `_pending_reads`.
- `--max-datagrams-per-turn` -- the per-turn datagram ceiling in
  `_pending_reads`. Small values exercise the `_read_again` yield path.

## Oracles

- **Payload integrity** -- the server reads a 4-byte header (client id +
  sequence number), regenerates the expected keystream for that position, and
  compares. A mismatch is corruption.
- **Crash / assert** -- debug build, asserts on.

On success (every invariant holds) the engine prints `RESULT ...` then `PASS`
and returns. Anything short of that prints `FAIL` and exits non-zero.

## Building and running

Build the engine with the orchestrator's `--ponyc` option (which compiles
from source) or directly with `ponyc`:

```bash
ponyc -d -o build/debug test/rt-stress/udp-swarm     # -> build/debug/udp-swarm
```

Run the engine directly for a single workload:

```bash
build/debug/udp-swarm --datagrams 100 --clients 4 --payload-size 256 \
  --batch-size 10
```

Every flag is checked against a schema and its valid range. `--help` lists them.

## Running the swarm

The orchestrator draws one workload per seed and runs the prebuilt engine once
per seed. It does not compile; point `--binary` at the engine you built above.

```bash
python3 test/rt-stress/udp-swarm/orchestrate_udp.py \
  --binary build/debug/udp-swarm --count 50 --out ~/tmp/udp-swarm-out
```

The draw is stable per seed. Selectors:

- `--count N` / `--start S` -- run N seeds from S.
- `--seeds A,B,C` -- run specific seeds.
- `--replay N` -- reproduce seed N's workload.
- `--budget-seconds N` -- run seeds from `--start` until N seconds pass.
- `--lldb <path>` -- run each seed under lldb so a crash leaves a backtrace.

A run is a failure only if it crashes, mismatches, or hangs. A failure writes
`bundle-<seed>.json` to `--out`. A healthy run is never failed for running long:
one still making progress at the `--timeout-seconds` backstop is reported
`incomplete`, not failed.

`orchestrate_udp_test.py` covers the pure pieces: `python3 orchestrate_udp_test.py`.
