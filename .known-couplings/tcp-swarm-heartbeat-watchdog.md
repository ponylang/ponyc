# The tcp-swarm engine's heartbeat cadence is coupled to the orchestrator's no-progress watchdog

The tcp-swarm orchestrator (`test/rt-stress/tcp-swarm/orchestrate_tcp.py`) kills a
run that produces NO output for `DEFAULT_NO_PROGRESS_SECONDS` (the
`_watch_for_progress` watchdog), rather than on total wall-clock length -- so a
slow-but-advancing run finishes and only a genuinely stuck one (a hang, a lost
connection, a shutdown that never completes) is killed. That only tells a hang
from a slow run because the engine (`test/rt-stress/tcp-swarm/main.pony`) emits a
flushed `HEARTBEAT` line as connections complete: the `Spawner` prints one every
`_heartbeat_every` completions, where `_heartbeat_every = max(1, connections / 50)`
-- about 50 heartbeats per run regardless of size.

The two are a pair:

- The worst gap between heartbeats on a live run MUST stay well under
  `DEFAULT_NO_PROGRESS_SECONDS`, or the watchdog false-fires on a healthy run. At
  ~50 heartbeats per run, the gap is `run_time / 50`, so a false-fire needs a run
  longer than ~50x the window (far past the per-run timeout). Change either the
  cadence (`_heartbeat_every` in main.pony) or the threshold (`orchestrate_tcp.py`)
  and re-check that the slowest healthy run still heartbeats inside the window.
- The heartbeat MUST be flushed (`@fflush(@pony_os_stdout())`). A bare `@printf`
  to a piped stdout is block-buffered, so the line would not reach the watchdog
  until the buffer fills or the program exits, and the watchdog would false-fire
  on every healthy long run.
- A run that STALLS (a connection that never completes) emits no further
  heartbeat, so the watchdog fires -- which is the point: that is a hang.

Run: a large tcp-swarm run (e.g. `./tcp_swarm --connections 100000 ...`),
confirming `HEARTBEAT` lines stream during the run rather than bunching at exit.
