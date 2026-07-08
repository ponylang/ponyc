# The generative engine's heartbeat cadence is coupled to the orchestrator's no-progress watchdog

The normal-mode stress orchestrator
(`test/rt-stress/generative/stress_common.py`) kills a run when it produces NO
output for `DEFAULT_NORMAL_NO_PROGRESS_SECONDS` (the `_watch_for_progress`
watchdog), instead of on total wall-clock length. That only tells a hang from a
slow run because the engine (`test/rt-stress/generative/main.pony`) emits a
flushed `HEARTBEAT` line as it makes progress: the mesh `Pinger` and the
backpressure `Consumer` call `_Heartbeat.emit()` every `_Heartbeat.interval(...)`
messages they receive. The two numbers are a pair:

- The worst gap between heartbeats on a live run MUST stay well under
  `DEFAULT_NORMAL_NO_PROGRESS_SECONDS`, or the watchdog false-fires on a healthy
  run. The cadence is set by `_Heartbeat._target` (heartbeats per emitter) and
  `_Heartbeat._min` (the gate below which a run is too small to bother) in
  `main.pony`; the threshold is in `stress_common.py`. Change either — the engine's
  cadence/gate, or the threshold — and re-check that the slowest drawable live run
  still heartbeats comfortably inside the window.
- The heartbeat MUST be flushed (`@fflush(@pony_os_stdout())`). A bare `@printf`
  to a piped stdout is block-buffered, so the line would not reach the watchdog
  until the buffer fills or the program exits — the watchdog would then false-fire
  on every healthy long run. Verified: a flushed heartbeat arrives in real time
  both directly and under lldb (normal mode runs the engine under lldb).
- The `cyclic` workload emits NO heartbeat on purpose: its largest draw finishes
  in well under the no-progress window (~8s measured, single thread), so it never
  needs one. If a future cyclic-bucket bump pushed its worst-case runtime toward
  `DEFAULT_NORMAL_NO_PROGRESS_SECONDS`, it would need a heartbeat (the persistent
  `CyclicCollector` is the place — its workers are ephemeral) or a larger window.
- The `iso` workload likewise emits no heartbeat: its `chains` are clamped to the
  acquire-flood budget (`ISO_ACQUIRE_BUDGET`), so its total message count is capped
  near ~100k (well below `_Heartbeat._min`) and its worst-case run finishes in well
  under a second (measured ~0.2s, 16 threads, at the calibrated budget across every
  drawn graph shape). Like cyclic it never approaches the window; if the acquire
  budget were ever raised enough to push its runtime toward
  `DEFAULT_NORMAL_NO_PROGRESS_SECONDS`, the `Carrier` would need a heartbeat (the
  persistent `Dispatcher` is the place — its carriers stay live, so either works).
- The heartbeat is gated OFF for small runs (`_Heartbeat._min`), which keeps
  SYSTEMATIC runs (all far below it) heartbeat-free; systematic also keeps the flat
  total-time watchdog (`no_progress_seconds=None`), not this one — that total-time
  watchdog is the only hang detector for systematic runs, and this change must not
  weaken it.

Run: the orchestrator unit tests
(`python test/rt-stress/generative/stress_common_test.py` —
`test_watchdog_should_kill`, `test_watch_for_progress`) plus, for the engine side,
a large mesh or backpressure run, confirming `HEARTBEAT` lines stream during the
run (not bunched at exit).
