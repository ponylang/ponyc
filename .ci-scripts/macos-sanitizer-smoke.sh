#!/bin/bash
# Native sanitizer smoke test for macOS. Invoked from
# .github/workflows/ponyc-weekly-checks.yml. Expects to run from the ponyc
# source root with LLVM already built in build/libs.
#
# The macOS sanitizer link path through embedded ld64.lld was previously
# uncovered — native sanitizer builds fell back to the legacy compiler driver.
# This builds an instrumented ponyc plus the self-hosted tools, then links and
# runs a small program whose allocations engage AddressSanitizer, asserting a
# clean exit. The link is the thing under test: a broken splice shows up as
# undefined sanitizer symbols at link time, not as a runtime surprise.
#
# ASAN_OPTIONS/UBSAN_OPTIONS are exported for the build too, not just the run,
# because the instrumented ponyc executes during its own build (compiling the
# tools). detect_leaks=0 matches the Linux weekly. detect_container_overflow=0
# suppresses false-positive container-overflow reports from Apple's libc++
# container annotations when the instrumented ponyc shares std::vector/
# std::string with the (non-instrumented) vendored LLVM.
set -eu

export ASAN_OPTIONS=detect_leaks=0:detect_container_overflow=0
export UBSAN_OPTIONS=print_stacktrace=0
if [ -x "$PWD/build/libs/bin/llvm-symbolizer" ]; then
  ASAN_OPTIONS="$ASAN_OPTIONS:external_symbolizer_path=$PWD/build/libs/bin/llvm-symbolizer"
  UBSAN_OPTIONS="$UBSAN_OPTIONS:external_symbolizer_path=$PWD/build/libs/bin/llvm-symbolizer"
fi

# Clean + reconfigure debug from scratch. If a prior non-sanitizer
# config=debug build left a build dir behind, reconfiguring it in place
# re-runs the standalone-library rule over a read-only leftover (libc++.a is
# 0444, so the copied libcpp.a is too) which fails. clean is config-scoped
# and defaults to release, so pass config=debug to remove the debug
# build/output dirs; the prebuilt LLVM in build/libs is untouched.
make clean config=debug
make configure config=debug \
  use=pool_memalign,address_sanitizer,undefined_behavior_sanitizer
# Full build: this compiles the instrumented ponyc and links the self-hosted
# tools (pony-lsp/pony-lint/pony-doc) under sanitizers, exercising the
# embedded ld64.lld link of programs that bundle the C++ runtime
# (libponyc-standalone.a).
make build config=debug

# The sanitizer suffix order is fixed by CMake (address, then undefined, then
# pool_memalign), independent of the use= order above; derive the output dir
# rather than hardcode it.
out=$(find build -maxdepth 1 -type d -name 'debug-*address_sanitizer*pool_memalign' | head -1)
if [ -z "$out" ] || [ ! -x "$out/ponyc" ]; then
  echo "FAIL: sanitizer build output directory not found"
  exit 1
fi

smoke=/tmp/sanitizer-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    // Allocate enough to exercise the AddressSanitizer-instrumented libponyrt
    // allocator, then exit clean.
    let a = Array[U64](4096)
    var i: USize = 0
    while i < 4096 do
      a.push(i.u64())
      i = i + 1
    end
    env.out.print("sanitizer smoke ok size=" + a.size().string())
PONY

# Link via embedded ld64.lld (the captured sanitizer fragment is spliced here)
# and run. A broken splice fails at link time with undefined __asan_*/__ubsan_*.
PONYPATH="$PWD/packages" "$out/ponyc" --debug -o "$smoke" "$smoke"
out_text=$("$smoke/sanitizer-smoke")
echo "$out_text"
echo "$out_text" | grep -q "sanitizer smoke ok size=4096"

# The self-hosted tools link libponyc-standalone.a (which bundles libc++ on
# macOS); confirm one runs clean under the sanitizer runtime, proving the tool
# link path works and not just that it linked.
"$out/pony-lint" --version
