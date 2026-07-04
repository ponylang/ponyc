# The tcp-swarm memory budget's cost model tracks the engine's per-object and per-read-buffer allocation

The tcp-swarm orchestrator (`test/rt-stress/tcp-swarm/orchestrate_tcp.py`) draws
each workload against a memory budget (`MEM_BUDGET_BYTES`) so no draw exceeds the
`RLIMIT_AS` cap the run is launched under. A draw over the cap is killed by the
runtime's own out-of-memory abort (`ponyint_virt_alloc`), which reads like a runtime
crash but is really the test asking for more memory than its cap allows -- a false
failure. The budget is what prevents it.

The budget uses `est_peak_bytes`, which estimates the engine's peak memory from a few
measured coefficients:

- `MEM_OBJ_BYTES * concurrency * messages * writev_chunks` -- the pending vectored-write
  buffer objects. `_Keystream.make_chunks` (`main.pony`) allocates `writev_chunks`
  `Array[U8]` objects per message, *regardless of payload* (the extras are zero-length
  when the chunk count exceeds the payload), and the `Spawner` keeps `concurrency`
  connections in flight. The term assumes exactly that object count and that live-set
  size.
- `MEM_RB_FACTOR * concurrency * read_buffer` and `connections * read_buffer` -- the
  read buffers, one per live connection (both endpoints) plus a conservative churn
  term. Sized by `--read-buffer-size`.

If the engine's per-object cost, the read buffer, or `TCPConnection`'s buffering
changes -- e.g. `make_chunks` starts coalescing empty chunks, or the read path changes
how much it holds -- the coefficients drift. Under-estimating lets an over-cap draw
through (the false OOM this exists to prevent); over-estimating silently over-trims and
loses swarm coverage. The coupling is silent: `orchestrate_tcp_test.py` checks that
`est_peak_bytes` stays within budget, NOT that `est_peak_bytes` matches the engine's
real memory -- so a drift passes the unit test and only shows up as a CI OOM (or as
quietly shrunken draws). The constants are first-CI-run calibration items; the churn
term's shape (`connections * read_buffer`) is an unvalidated hypothesis (the CI OOM
seeds with huge connection counts could not be reproduced on the slow local loopback).

Run: re-measure the engine's peak memory across the levers and re-fit the constants --
build the engine debug and, polling `/proc/<pid>/status` `VmPeak`/`VmHWM`, sweep e.g.
`./tcp_swarm --write-shape writev --writev-chunks 2048 --payload-size 8 --messages 8
--concurrency {64,128} --connections 2000` and confirm `est_peak_bytes` still bounds
the observed peak with margin. There is no automated regression test -- the engine
run is loopback-speed and memory-heavy, so it stays a manual re-measure.
