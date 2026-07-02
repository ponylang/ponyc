#!/bin/sh
# Assert that ponyc rejects the unsupported `use=` build options on DragonFly.
# Runs inside the DragonFly VM, invoked from .github/workflows/ponyc-tier3.yml.
# POSIX sh (DragonFly's /bin/sh). Expects to run from the ponyc source root.
#
# DragonFly builds ponyc with the gcc13 package, whose toolchain ships no
# AddressSanitizer/ThreadSanitizer/UndefinedBehaviorSanitizer runtime (GCC's
# libsanitizer has no DragonFly target), so those use= options can't link there;
# and DragonFly can't build a working dtrace-instrumented runtime, so use=dtrace
# is rejected too. `gmake configure` rejects them with a clear error at
# make-parse time, before `gmake libs` runs, so this assertion needs no LLVM
# build or gcc13 toolchain. Check each fails AND fails for the documented reason:
# a typo in the OS string would otherwise silently regress to the confusing
# partway-through build failure the guards exist to prevent. (coverage and
# valgrind aren't rejected on DragonFly.)
set -eu

for u in address_sanitizer thread_sanitizer undefined_behavior_sanitizer dtrace; do
  if out=$(gmake configure use="$u" 2>&1); then
    echo "FAIL: use=$u was not rejected on DragonFly"
    exit 1
  fi
  if ! printf '%s\n' "$out" | grep -q "not supported on DragonFly"; then
    echo "FAIL: use=$u failed for an unexpected reason:"
    printf '%s\n' "$out"
    exit 1
  fi
done
echo "unsupported use= options correctly rejected on DragonFly"
