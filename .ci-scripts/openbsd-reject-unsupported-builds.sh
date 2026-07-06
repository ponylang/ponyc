#!/bin/sh
# Assert that ponyc rejects the unsupported `use=` build options on OpenBSD.
# Runs inside the OpenBSD VM, invoked from .github/workflows/ponyc-tier3.yml.
# POSIX sh (OpenBSD's /bin/sh). Expects to run from the ponyc source root.
#
# OpenBSD's base toolchain ships no AddressSanitizer/ThreadSanitizer runtime and
# only the minimal (not standalone) UndefinedBehaviorSanitizer runtime, has an
# incomplete profiling runtime, and has no Valgrind port; and OpenBSD ships no
# DTrace-compatible probe-generation tool. So these use= options can't link,
# compile, or run there. cmake's use= validation rejects them with a clear error,
# and it runs before LLVM is required, so this assertion needs no LLVM build.
# Check each fails AND fails for the documented reason: a typo in
# the OS string would otherwise silently regress to the confusing
# partway-through build failure the guards exist to prevent.
set -eu

# Validation runs inside cmake configure (cmake/PonyUses.cmake), which caches a
# compiler in the build dir it configures. Configure directly with cmake into a
# throwaway build dir -- not a preset (the presets pin clang) and never the
# shared build_release the real build reuses, so this validation-only step never
# pins the build's compiler. OpenBSD runs both the reject and the build with base
# clang, so a poisoned dir would not currently mismatch, but keeping the probe
# isolated avoids the compiler-poisoning class of bug that bit DragonFly (base
# gcc 8 reject vs gcc13 build). See .known-couplings/use-option-validation.md.
for u in address_sanitizer thread_sanitizer undefined_behavior_sanitizer coverage valgrind dtrace; do
  rm -rf build/build_reject-probe
  if out=$(cmake -B build/build_reject-probe -S . -DPONY_USES="$u" 2>&1); then
    echo "FAIL: use=$u was not rejected on OpenBSD"
    exit 1
  fi
  if ! printf '%s\n' "$out" | grep -q "not supported on OpenBSD"; then
    echo "FAIL: use=$u failed for an unexpected reason:"
    printf '%s\n' "$out"
    exit 1
  fi
done
rm -rf build/build_reject-probe
echo "unsupported use= options correctly rejected on OpenBSD"
