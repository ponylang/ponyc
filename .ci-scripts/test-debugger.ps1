# Print the debugger command a Windows CI leg runs the tests under, to get a
# backtrace on a crash. The Windows counterpart to test-debugger.sh. A workflow
# does:
#
#   $env:PONY_TEST_DEBUGGER = & .ci-scripts/test-debugger.ps1 lldb
#   ctest --preset windows-x86-64-debug -L ci-core
#
# cmake's test wrappers split PONY_TEST_DEBUGGER with shell/UNIX_COMMAND quoting,
# so the quoted sub-commands below survive as single arguments. The lldb path uses
# forward slashes because a backslash is an escape character under cmake's
# UNIX_COMMAND parsing (separate_arguments in RunTest.cmake / RunStdlibTest.cmake).
# lldb ends with `--`, so the test binary and its arguments follow.
#
# Windows has no SIGUSR2, so this mirrors the simpler lldb line the Windows build
# used before the cmake migration, not test-debugger.sh's signal-handling form.
param([string]$Debugger = "")

switch ($Debugger) {
  "lldb" {
    Write-Output 'C:/msys64/mingw64/bin/lldb.exe --batch --one-line run --one-line-on-crash "frame variable" --one-line-on-crash "register read" --one-line-on-crash "bt all" --one-line-on-crash "quit 1" --'
  }
  default {
    [Console]::Error.WriteLine("usage: $($MyInvocation.MyCommand.Name) {lldb}")
    exit 1
  }
}
