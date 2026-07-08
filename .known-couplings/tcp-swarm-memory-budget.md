# The tcp-swarm memory budget's cost model tracks the engine's allocation

The tcp-swarm orchestrator (`test/rt-stress/tcp-swarm/orchestrate_tcp.py`) trims each
workload's memory-driving levers against a budget (`est_peak_bytes` vs `MEM_BUDGET_BYTES`)
so a drawn run stays under the `RLIMIT_AS` cap each seed launches with. That estimate is a
cost model of the engine's (`test/rt-stress/tcp-swarm/main.pony`) actual allocation:
`MEM_OBJ_BYTES * concurrency * messages * writev_chunks` for the pending vectored-write
buffer objects `_Keystream.make_chunks` allocates, plus per-connection read-buffer terms
sized by `--read-buffer-size`.

The coupling: change how the engine allocates — the per-message buffer objects, the
per-connection read buffers — without updating the cost model, and the two silently
disagree. It surfaces two ways, neither caught by a test:

- estimate runs LOW → a draw exceeds the cap and the runtime's own out-of-memory abort
  (`ponyint_virt_alloc`) kills it, reading like a runtime crash. Largely cushioned today by
  a deliberately slack 14 GiB cap, but a large enough drift still crosses it.
- estimate runs HIGH → the levers trim harder than needed and the draw silently shrinks,
  reducing coverage with no signal at all.

`orchestrate_tcp_test.py` checks only that `est_peak_bytes` stays within budget, NOT that it
matches real memory, so drift passes the unit test.

Run: no automated regression test — the engine run is loopback-speed and memory-heavy. To
re-tighten the model after an allocation change, build the engine debug and poll
`/proc/<pid>/status` `VmPeak` on the failing seed's config (`./tcp_swarm --connections 53268
--concurrency 8 --payload-size 1024 --messages 37 --write-shape writev --writev-chunks 2048
--read-buffer-size 1024 --ponynoscale --ponymaxthreads 2`), comparing `VmPeak` to
`est_peak_bytes` (peak VIRTUAL runs several× over live bytes because the pool allocator
reserves in large `MAP_NORESERVE` arenas, and grows further when CPU-starved — pin to 2 cores
to reproduce the CI OOM). `lori` carries a copy of this model against its own TCP stack.
