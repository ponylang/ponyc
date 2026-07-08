# The generative timeout-backtrace capture depends on lldb's process tree and SIGABRT staying unhandled by lldb's pass-through

When a generative stress run under lldb hits its watchdog, the orchestrator
(`test/rt-stress/generative/stress_common.py`, `lldb_timeout_abort`) gets a
backtrace out of the hung engine by sending SIGABRT to the *inferior*: lldb
stops on the signal, batch mode treats the stop as a crash and runs the same
on-crash commands a real crash gets (`frame variable`, `bt all`, `quit 1`), and
the backtrace flushes into the captured output as lldb exits. Two non-obvious
dependencies make that work, and both break *silently* — a broken capture just
means timeouts go back to recording nothing:

- **SIGABRT must stay OUT of `lldb_argv`'s signal pass-through list.** The list
  (`process handle <sig> --pass true --stop false`) exists so lldb doesn't stop
  on the runtime's own signals (SIGINT, SIGUSR2). Adding SIGABRT to it would
  make lldb hand the abort straight to the engine — no stop, no backtrace, the
  inferior just dies.
- **The inferior's pid comes from a `ps` walk of lldb's process tree, not from
  lldb's output.** lldb buffers its own messages when its stdout is a pipe and
  flushes them only at exit (verified empirically; `stdbuf` does not override
  it), so mid-run there is no `Process N launched` line to parse. The walk
  (`_lldb_inferior_pid`) descends from lldb through its helper processes and
  returns the first descendant that isn't one — it depends on the helpers being
  recognizable by name (`lldb-server` on Linux, `debugserver` on macOS) and on
  `ps -eo pid=,ppid=,comm=` working (POSIX). A new lldb spawning a differently
  named helper would make the walk return the helper as "the inferior", abort
  the wrong process, and lose the backtrace.

Run: `python3 test/rt-stress/generative/stress_common_test.py` — the pass-through
list is guarded textually (`test_lldb_argv`), the walk by unit tests
(`test_lldb_inferior_pid`), and the whole chain behaviorally by the real-lldb
integration test (`test_lldb_timeout_capture_integration`), which runs per-PR
because lint-python.yml's `test-rt-stress` job installs lldb (remove that
install and the integration test silently skips on the stock runner).
