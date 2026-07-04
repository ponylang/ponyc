# The tcp-swarm multi-batch write coverage depends on pony_os_writev_max()'s POSIX value

The tcp-swarm stress test's `--writev-chunks` lever
(`test/rt-stress/tcp-swarm/`) exists to exercise `TCPConnection`'s multi-batch
send path and its mid-write yield (`--yield-after-writing`). Those only run when a
single `writev` queues more buffers than one `@pony_os_writev_max()` syscall sends
-- `IOV_MAX` (1024 on Linux and macOS) on POSIX, `1` on Windows (defined in
`src/libponyrt/lang/io.c`). The orchestrator picks
`WRITEV_CHUNKS = [4, 64, 2048]`; the `2048` is chosen to exceed the POSIX
`IOV_MAX` so that, with a payload at least that large, the multi-batch path is
reached on POSIX.

The coupling is silent: `orchestrate_tcp_test.py`'s "multi-batch writev is
reachable" assertion checks the *draw* (writev-chunks > 1024 and payload >= it),
not the runtime's actual batch size. The multi-batch path needs a `writev` to
queue *strictly more* buffers than one syscall sends, so it needs
`WRITEV_CHUNKS`'s large value to stay strictly above `pony_os_writev_max()`. If
that value on POSIX ever rose **to 2048 or higher**, or `WRITEV_CHUNKS`'s large
value were lowered to at or below the POSIX `IOV_MAX`, the swarm would still pass
its unit test but would stop exercising the multi-batch send path and the
mid-write yield on the POSIX CI runs, and no test would catch it. Keep the large
`WRITEV_CHUNKS` value
comfortably above `pony_os_writev_max()`'s POSIX return. (Windows is unaffected:
its `writev_max` of 1 makes any multi-chunk writev multi-batch -- see
`writev-max-net-files.md` for the separate reason that value must stay 1.)

Run: the tcp-swarm engine with a multi-batch config on POSIX, confirming the
multi-batch branch is taken -- e.g. instrument `_pending_writes`'s
`num_to_send = writev_batch_size` branch, build the engine debug, and run
`./tcp_swarm --write-shape writev --writev-chunks 2048 --payload-size 4096
--yield-after-writing 64 --connections 40 --concurrency 8`; the branch (and the
mid-write yield) should fire.
