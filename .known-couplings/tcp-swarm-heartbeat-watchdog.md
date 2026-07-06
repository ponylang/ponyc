# The tcp-swarm engine's heartbeat is coupled to the orchestrator's progress watchdog

The tcp-swarm orchestrator (`test/rt-stress/tcp-swarm/orchestrate_tcp.py`) decides a
run is a hang -- and fails it -- when the completed-connection count stops advancing
for `DEFAULT_NO_PROGRESS_SECONDS` (the `_watch_for_progress` watchdog). It does NOT
fail a run for running long: a healthy run that keeps completing connections runs to
completion, and one still advancing when it passes the absolute `--timeout-seconds` is
stopped as a non-failing backstop (outcome "incomplete"), not a hang. Telling the two
apart depends on the engine (`test/rt-stress/tcp-swarm/main.pony`): the `Spawner`
prints a flushed `HEARTBEAT done=<completed> of <total>` line on a fixed wall-clock
timer (`_HeartbeatTimer`, every 5s), and the watchdog reads the `done=` count -- a run
makes progress when that number rises.

The engine and the orchestrator are a pair:

- The heartbeat is on a TIMER, not per-completion, so the line keeps coming at a fixed
  cadence no matter how slowly or quickly connections complete. The watchdog measures
  progress by the `done=` value rising between heartbeats, not by any line appearing --
  so a slow-but-advancing run never looks stalled, and a run whose count freezes (a
  connection that never completes, a teardown that never finishes) is a hang even though
  its timer heartbeat keeps printing.
- Mid-run liveness assumes the slowest SINGLE connection completes well within the
  window. The watchdog reads progress from connections completing; if one connection's
  whole exchange took longer than the window while the others had already drained,
  `done` would freeze and a healthy run would read as a hang. This holds because the
  draw bounds per-connection work: `messages` (<= 64) * `payload-size` (<= 65536) is at
  most ~4 MiB, and concurrency is >= 8 so several connections are always in flight.
  Raising the message/payload maxima, dropping the concurrency floor, or loosening
  `clamp_run` in `orchestrate_tcp.py` could let a single connection cross the window --
  re-check this bound there (those constants carry a pointer back here).
- The timer interval (5s in main.pony) MUST stay well under
  `DEFAULT_NO_PROGRESS_SECONDS` (300s). If the interval reaches the window, a healthy run
  is killed before its first heartbeat -- the startup gap alone false-fires (verified:
  running with `--no-progress-seconds 3` against the 5s interval reports a false hang).
  The 60x margin also covers timer delay under a fully CPU-bound single scheduler thread
  (measured: the 5s timer fires within ~0.3s even at `--ponymaxthreads 1`, pinned).
  Change either the interval (`_HeartbeatTimer` in main.pony) or the window
  (`orchestrate_tcp.py`) and re-check that the interval stays comfortably under it.
- The heartbeat line MUST carry `done=` and MUST be flushed
  (`@fflush(@pony_os_stdout())`). The watchdog parses `HEARTBEAT done=(\d+)` (`_DONE_RE`
  / `_parse_done`); a renamed field or a block-buffered line the orchestrator can't read
  in time would make every healthy run look like a hang.
- The engine MUST dispose its `Timers` when the run finishes (`_try_finish`). A live
  repeating timer is a noisy ASIO event that would keep the program from quiescing, so a
  passing run would hang on exit -- which the watchdog would then (correctly, for that
  broken state) report as a hang.

Run: a large tcp-swarm run (e.g. `./tcp_swarm --connections 100000 ...`), confirming
`HEARTBEAT done=` lines stream on the timer during the run and `done` rises; and a small
run, confirming it prints `PASS` and exits (the timer is disposed, so it quiesces).
