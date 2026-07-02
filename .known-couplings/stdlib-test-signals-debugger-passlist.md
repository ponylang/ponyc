# Signals raised by stdlib tests must be in the debugger wrapper pass-lists

The stdlib test suites run under `.ci-scripts/run-under-debugger.bash` on the
glibc Linux CI legs (per-PR and weekly; today invoked through the Makefile's
`debuggercmd`): an lldb or gdb wrapper whose pass-lists name the signals the
debugger must forward without stopping (`process handle <SIG> --pass true
--stop false` / `handle <SIG> nostop pass`). A stdlib test that raises a
signal missing from those lists doesn't fail — the debugger stops on the
first delivery, runs the on-crash commands, and exits 1, killing the whole
suite leg. Nothing near `packages/signals/_test.pony` reveals this: the tests
pass everywhere except under the wrapper. This has bitten twice — commit
72788dbb0 moved the original signal tests to SIGINT for exactly this reason,
and the SIGTERM-based subscriber-limit test later reintroduced it. The lists
currently cover SIGINT, SIGTERM, and SIGUSR2 (the scheduler wake signal).

Run: `make test-stdlib-debug usedebugger=lldb` on Linux (the same invocation
CI uses) after adding any test that raises a new signal number.
