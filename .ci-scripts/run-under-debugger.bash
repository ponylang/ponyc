#!/bin/bash
# Run a command under lldb or gdb the way CI runs the test suites: batch
# mode, backtraces on crash, and benign runtime signals forwarded to the
# program instead of stopping the debugger.
#
# Usage: run-under-debugger.bash <lldb|gdb binary> <command> [args...]
#
# Coupled: every signal the stdlib tests raise must be in BOTH pass-lists
# below — see .known-couplings/stdlib-test-signals-debugger-passlist.md. A
# signal missing from the lists doesn't fail the tests; it stops the
# debugger, which runs its on-crash commands and exits 1.

set -o errexit
set -o nounset

dbg="$1"
shift

case "$dbg" in
  *lldb*)
    exec "$dbg" --batch \
      --one-line "breakpoint set --name main" \
      --one-line run \
      --one-line "process handle SIGINT --pass true --stop false" \
      --one-line "process handle SIGTERM --pass true --stop false" \
      --one-line "process handle SIGUSR2 --pass true --stop false" \
      --one-line "thread continue" \
      --one-line-on-crash "frame variable" \
      --one-line-on-crash "register read" \
      --one-line-on-crash "bt all" \
      --one-line-on-crash "quit 1" \
      -- "$@"
    ;;
  *gdb*)
    exec "$dbg" --quiet --batch --return-child-result \
      --eval-command="set confirm off" \
      --eval-command="set pagination off" \
      --eval-command="handle SIGINT nostop pass" \
      --eval-command="handle SIGTERM nostop pass" \
      --eval-command="handle SIGUSR2 nostop pass" \
      --eval-command=run \
      --eval-command="info args" \
      --eval-command="info locals" \
      --eval-command="info registers" \
      --eval-command="thread apply all bt full" \
      --eval-command=quit \
      --args "$@"
    ;;
  *)
    echo "run-under-debugger: unknown debugger '$dbg'" >&2
    exit 1
    ;;
esac
