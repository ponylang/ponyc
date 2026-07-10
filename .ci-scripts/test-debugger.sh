#!/usr/bin/env bash
# Print the debugger command a CI leg runs the tests under, to get a backtrace on a
# crash. The specific flags are a CI/environment concern, so they live here, not in
# cmake. A workflow does:
#
#   export PONY_TEST_DEBUGGER="$(.ci-scripts/test-debugger.sh lldb)"
#   ctest --preset x86-64-debug -L ci-core
#
# cmake's test wrappers read PONY_TEST_DEBUGGER and split it with shell quoting
# rules, so the quoted sub-commands below survive as single arguments. lldb ends
# with `--` and gdb with `--args`, so the test binary and its arguments follow.
#
# The signals below are ones a Pony program takes in normal operation, so a
# debugger that stops on them turns a passing run into a reported crash. SIGPIPE
# is on the list even though the runtime sets it to SIG_IGN: a debugger stops on
# an ignored signal too, seeing it before the process's disposition applies.
set -euo pipefail

case "${1:-}" in
  lldb)
    echo 'lldb --batch --one-line "breakpoint set --name main" --one-line run --one-line "process handle SIGINT --pass true --stop false" --one-line "process handle SIGUSR2 --pass true --stop false" --one-line "process handle SIGPIPE --pass true --stop false" --one-line "thread continue" --one-line-on-crash "frame variable" --one-line-on-crash "register read" --one-line-on-crash "bt all" --one-line-on-crash "quit 1" --'
    ;;
  gdb)
    echo 'gdb --quiet --batch --return-child-result --eval-command="set confirm off" --eval-command="set pagination off" --eval-command="handle SIGINT nostop pass" --eval-command="handle SIGUSR2 nostop pass" --eval-command="handle SIGPIPE nostop pass" --eval-command=run --eval-command="info args" --eval-command="info locals" --eval-command="info registers" --eval-command="thread apply all bt full" --eval-command=quit --args'
    ;;
  *)
    echo "usage: ${0} {lldb|gdb}" >&2
    exit 1
    ;;
esac
