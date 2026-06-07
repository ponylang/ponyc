#!/bin/sh
# Assert that ponyc rejects the unsupported `use=` build options on OpenBSD.
# Runs inside the OpenBSD VM, invoked from .github/workflows/ponyc-tier3.yml.
# POSIX sh (OpenBSD's /bin/sh). Expects to run from the ponyc source root.
#
# OpenBSD's base toolchain ships no AddressSanitizer/ThreadSanitizer runtime and
# only the minimal (not standalone) UndefinedBehaviorSanitizer runtime, has an
# incomplete profiling runtime, and has no Valgrind port, so these use= options
# can't link or compile there. `gmake configure` rejects them with a clear error
# at make-parse time, before `gmake libs` runs, so this assertion needs no LLVM
# build. Check each fails AND fails for the documented reason: a typo in the OS
# string would otherwise silently regress to the confusing partway-through build
# failure the guards exist to prevent. (use=dtrace is also rejected on OpenBSD,
# but by its own which-dtrace check with a different message.)
set -eu

for u in address_sanitizer thread_sanitizer undefined_behavior_sanitizer coverage valgrind; do
  if out=$(gmake configure use="$u" 2>&1); then
    echo "FAIL: use=$u was not rejected on OpenBSD"
    exit 1
  fi
  if ! printf '%s\n' "$out" | grep -q "not supported on OpenBSD"; then
    echo "FAIL: use=$u failed for an unexpected reason:"
    printf '%s\n' "$out"
    exit 1
  fi
done
echo "unsupported use= options correctly rejected on OpenBSD"
