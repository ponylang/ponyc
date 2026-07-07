# The tcp-swarm memory budget's cost model tracks the engine's per-object and per-read-buffer allocation

The tcp-swarm orchestrator (`test/rt-stress/tcp-swarm/orchestrate_tcp.py`) draws each
workload against a memory budget (`MEM_BUDGET_BYTES`) that trims the memory-driving
levers, aiming to keep a run under the `RLIMIT_AS` cap each seed launches under. A draw
over the cap is killed by the runtime's own out-of-memory abort (`ponyint_virt_alloc`),
which reads like a runtime crash but is really the test asking for more address space than
its cap allows -- a false failure.

The budget does NOT bound the quantity the cap limits. `RLIMIT_AS` caps VIRTUAL address
space; `est_peak_bytes` estimates peak LIVE workload bytes:

- `MEM_OBJ_BYTES * concurrency * messages * writev_chunks` -- the pending vectored-write
  buffer objects. `_Keystream.make_chunks` (`main.pony`) allocates `writev_chunks`
  `Array[U8]` objects per message, *regardless of payload* (the extras are zero-length
  when the chunk count exceeds the payload), and the `Spawner` keeps `concurrency`
  connections in flight. The term assumes exactly that object count and that live-set
  size.
- `MEM_RB_FACTOR * concurrency * read_buffer` and `connections * read_buffer` -- the
  read buffers, one per live connection (both endpoints) plus a conservative churn
  term. Sized by `--read-buffer-size`.

Measured (perf-tester, ponyc `net` stack): the pool allocator reserves virtual in large
`MAP_NORESERVE` arenas and holds the high-water mark under the in-flight connection
backlog, so peak virtual runs ~4-7x over these live-byte terms -- and grows as the run is
CPU-starved (the deeper the backlog, the more the pool reserves). The failing 53k-connection
seed the budget estimated at ~1.2 GiB peaked ~5 GiB virtual on fast cores but ~8.4 GiB pinned
to 2 cores (~120 MiB RSS throughout); the 2-core run reproduces the CI OOM exactly, and the
old 8 GiB cap sat just under CI's ~8.4 GiB need. The churn term (`connections * read_buffer`)
is the weak one: it does not capture the backlog's pool reservation at all.

The fix was to raise the cap (`DEFAULT_MEM_LIMIT_MB` to 14 GiB) so its slack absorbs the
underestimate, NOT to re-fit the constants -- virtual reservation is a high-water artifact
that varies by environment and would need re-measuring per stack (lori carries a copy of
this whole model against its OWN TCP stack). So the coupling is now cushioned: a drift in
the engine's per-object or per-read-buffer cost no longer immediately false-OOMs, but a
large enough drift can still push a run's virtual past the raised cap. The coupling is
also silent -- `orchestrate_tcp_test.py` checks that `est_peak_bytes` stays within budget,
NOT that it matches real memory -- so drift passes the unit test and only shows up as a CI
OOM or as quietly shrunken draws.

Run: if you want the budget to be a tight bound again (rather than leaning on the cap's
slack), re-measure the engine's peak VIRTUAL across the levers and re-fit -- build the
engine debug and, polling `/proc/<pid>/status` `VmPeak`/`VmHWM`, run the failing seed's
config `./tcp_swarm --connections 53268 --concurrency 8 --payload-size 1024 --messages 37
--write-shape writev --writev-chunks 2048 --read-buffer-size 1024 --yield-after-reading 16384
--yield-after-writing 1024 --expect 0 --close graceful --ponynoscale --ponymaxthreads 2` and
compare `VmPeak` to `est_peak_bytes`. To see the CPU-starvation effect (and reproduce the CI
OOM), pin it to 2 cores (`taskset -c 0,1` or a `--cpus=2` container). There is no automated
regression test -- the engine run is loopback-speed and memory-heavy, so it stays a manual
re-measure.
