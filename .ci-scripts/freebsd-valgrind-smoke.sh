#!/bin/sh
# Valgrind smoke test for the FreeBSD tier-3 job. Runs inside the FreeBSD VM,
# invoked from .github/workflows/ponyc-tier3.yml. POSIX sh, not bash: FreeBSD's
# base system has no bash. Expects to run from the ponyc source root.
#
# use=valgrind annotates the classic pool so Valgrind can understand its
# custom allocation; the arena allocator (the default) carries no
# annotations and rejects the combination, so the smoke builds
# pool_classic,valgrind. FreeBSD ships a modern Valgrind port, so this is the
# one BSD where running a Pony program under Valgrind actually works — unlike
# DragonFly, whose Valgrind 3.15 is too old and hangs on the runtime's memory
# arena (https://github.com/ponylang/ponyc/issues/5435). This smoke guards two
# things:
#   1. the combination still builds — `cmake --build` compiles the annotated
#      runtime into ponyc; a build that can't compile or link is caught here.
#   2. a Pony program compiled with it runs to completion *under Valgrind*
#      without hanging — the DragonFly failure mode.
# It deliberately does NOT assert that Memcheck comes back clean: Pony's custom
# allocator isn't fully Valgrind-clean (Valgrind flags the pool allocator's
# intrusive free-list reuse at thread teardown), and getting there is a separate
# concern from "the build works and runs".
set -eu

# Configure the debug build from scratch with valgrind (remove the cmake build dir
# first); the prebuilt LLVM in build/libs is a separate dir and untouched, so it
# is not rebuilt. A from-scratch
# configure is required because the valgrind annotations differ from any plain
# debug build already present (the tier-3 job builds one, and the earlier smokes
# leave their own). This script is self-contained: it rebuilds from scratch, so
# it can run as its own CI step regardless of what came before.
rm -rf build/build_debug
cmake --preset debug -DPONY_USES=pool_classic,valgrind
# The `cmake --build` below is itself the first assertion: it compiles ponyc with
# the Valgrind-annotated runtime. A use=valgrind build that can't compile or
# link fails here, at build time.
cmake --build --preset debug

# The use options set PONY_OUTPUT_SUFFIX, so the build output lands in
# build/debug-valgrind-pool_classic. Derive it rather than hardcoding (the
# suffix order is CMake-determined and could grow more segments).
out=$(find build -maxdepth 1 -type d -name 'debug-valgrind*' | head -1)
if [ -z "$out" ] || [ ! -x "$out/ponyc" ]; then
  echo "FAIL: valgrind build output directory not found"
  exit 1
fi

smoke=/tmp/valgrind-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    env.out.print("valgrind smoke ok")
PONY

# Compile a program with the Valgrind-annotated ponyc — confirms it produces a
# working binary.
PONYPATH="$PWD/packages" "$out/ponyc" --debug -o "$smoke" "$smoke"

# Run the program UNDER Valgrind. The whole point: a Pony program runs to
# completion under Valgrind on FreeBSD, where DragonFly's older Valgrind hangs on
# the runtime's memory arena (#5435). `timeout` is the real assertion here — a
# hang surfaces as a non-zero (124) exit, not as missing output. We do not pass
# --error-exitcode or inspect Memcheck's error/leak summary on purpose (see the
# header): this checks that it builds and runs, not that it's Memcheck-clean.
log="$smoke/valgrind.log"
rc=0
timeout 300 valgrind "$smoke/valgrind-smoke" >"$smoke/out.txt" 2>"$log" || rc=$?
if [ "$rc" -ne 0 ]; then
  echo "FAIL: program did not run to completion under Valgrind (rc=$rc)"
  [ "$rc" -eq 124 ] && echo "(rc=124 = timed out: a DragonFly-style hang)"
  cat "$log"
  exit 1
fi

# The program ran to completion: assert its output.
if ! grep -q "valgrind smoke ok" "$smoke/out.txt"; then
  echo "FAIL: expected program output missing"
  cat "$smoke/out.txt"
  exit 1
fi

# Assert Valgrind actually instrumented the run (its summary always prints), so
# the smoke can't pass by accidentally running the binary outside Valgrind.
if ! grep -q "ERROR SUMMARY" "$log"; then
  echo "FAIL: no Valgrind ERROR SUMMARY — did the program run under Valgrind?"
  cat "$log"
  exit 1
fi

echo "valgrind smoke test passed"
