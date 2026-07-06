# The test harness reads pass/fail from the debugger's process-exit line, not its exit code

`cmake/RunTest.cmake` and `cmake/RunStdlibTest.cmake` run the test binaries. When `PONY_TEST_DEBUGGER` is set (the Windows CI jobs set it to lldb via `.ci-scripts/test-debugger.ps1`), they run the binary under the debugger to get a backtrace on a crash. They deliberately do NOT judge pass/fail by the debugger process's own exit code. Instead they capture the debugger's output and read the test binary's own status from the line `Process <pid> exited with status = <n>`, falling back to the debugger's exit code only when that line is absent (meaning the binary never ran).

This exists because the pinned Windows lldb — `mingw-w64-x86_64-lldb-22.1.4-1`, pinned in `.ci-scripts/windows-install-deps.ps1` — can crash on its own exit with `0xc0000374` (heap corruption in lldb) *after* the test binary has already exited cleanly. If the harness trusted the debugger's exit code, that crash-on-exit would be reported as a test failure even though every test passed. The old `make.ps1` did the same output-parsing (`Get-ProcessExitCodeFromLLDB`); the first cmake port dropped it and started trusting lldb's exit code, which turned the lldb crash into a spurious `stdlib-release` failure on Windows.

What breaks it:

- **Changing the debugger or its output format** (`test-debugger.ps1`, `test-debugger.sh`) so the line is no longer `Process <pid> exited with status = <n>` — the regex stops matching, and the harness silently falls back to the debugger's exit code, reintroducing the spurious-failure/masked-crash behavior.
- **Bumping the pinned lldb** (`windows-install-deps.ps1`) to a build whose exit-status wording differs, with the same effect.
- **Reverting the parse** back to the raw `RESULT_VARIABLE` — the debugger's exit code again gets a vote on pass/fail.

Run (where the coupling bites): the `ponyc: x86-64 Windows MSVC` job in `.github/workflows/pr.yml`, both the "Test with Debug Runtime" and "Test with Release Runtime" steps (`ctest ... -L ci-core`), which run `libponyc.tests`/`libponyrt.tests` and `stdlib-{debug,release}` under lldb.
