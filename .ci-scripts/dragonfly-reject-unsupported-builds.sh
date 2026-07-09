#!/bin/sh
# Assert that ponyc rejects the unsupported `use=` build options on DragonFly.
# Runs inside the DragonFly VM, invoked from .github/workflows/ponyc-tier3.yml.
# POSIX sh (DragonFly's /bin/sh). Expects to run from the ponyc source root.
#
# DragonFly builds ponyc with the gcc13 package, whose toolchain ships no
# AddressSanitizer/ThreadSanitizer/UndefinedBehaviorSanitizer runtime (GCC's
# libsanitizer has no DragonFly target), so those use= options can't link there;
# and DragonFly can't build a working dtrace-instrumented runtime, so use=dtrace
# is rejected too. cmake's use= validation rejects them with a clear error, and
# it runs before LLVM is required, so this assertion needs no LLVM build or gcc13
# toolchain. Check each fails AND fails for the documented
# reason: a typo in the OS string would otherwise silently regress to the
# confusing partway-through build failure the guards exist to prevent.
# (coverage and valgrind aren't rejected on DragonFly.)
set -eu

# Validation runs inside cmake configure (cmake/PonyUses.cmake), which caches a
# compiler in the build dir it configures. Configure directly with cmake into a
# throwaway build dir -- not a preset (the presets pin clang, which DragonFly's
# base toolchain lacks) and never the shared build_release the real build reuses.
# The base compiler is all the validation needs, and a poisoned build_release
# would pin the release build to the wrong toolchain. The options below must
# match the DragonFly rejections in cmake/PonyUses.cmake.
for u in address_sanitizer thread_sanitizer undefined_behavior_sanitizer dtrace; do
  rm -rf build/build_reject-probe
  if out=$(cmake -B build/build_reject-probe -S . -DPONY_USES="$u" 2>&1); then
    echo "FAIL: use=$u was not rejected on DragonFly"
    exit 1
  fi
  if ! printf '%s\n' "$out" | grep -q "not supported on DragonFly"; then
    echo "FAIL: use=$u failed for an unexpected reason:"
    printf '%s\n' "$out"
    exit 1
  fi
done
rm -rf build/build_reject-probe
echo "unsupported use= options correctly rejected on DragonFly"
