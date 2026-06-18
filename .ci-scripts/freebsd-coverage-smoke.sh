#!/bin/sh
# Coverage smoke test for the FreeBSD tier-3 job. Runs inside the FreeBSD VM,
# invoked from .github/workflows/ponyc-tier3.yml. POSIX sh, not bash: FreeBSD's
# base system has no bash. Expects to run from the ponyc source root.
#
# use=coverage is for working on ponyc itself: a coverage-instrumented ponyc
# records which parts of the compiler each run exercises. FreeBSD builds ponyc
# with its base clang (the -fprofile-instr-generate path). This smoke guards two
# things:
#   1. use=coverage still builds — it compiles the instrumented ponyc and links
#      the self-hosted tools (pony-lsp/pony-lint/pony-doc) plus a standalone
#      program, so a coverage build that can't link is caught here.
#   2. the coverage-instrumented ponyc actually emits profile data when it runs
#      (a .profraw) — the point of use=coverage, measuring ponyc's own coverage.
set -eu

# Clean + reconfigure debug from scratch with coverage. `clean` is config-scoped
# (pass config=debug so it removes the debug build/output dirs); the prebuilt
# LLVM in build/libs is untouched, so this does not rebuild LLVM. A from-scratch
# configure is required because the coverage instrumentation flags differ from
# the plain debug build the job already made. This runs after the sanitizer and
# dtrace smokes, which perform the same clean+reconfigure, so the order is
# deliberate.
gmake clean config=debug
gmake configure config=debug use=coverage
# The `gmake build` below is itself the first assertion: it compiles the
# instrumented ponyc and links the self-hosted tools (pony-lsp/pony-lint/
# pony-doc). A coverage build that can't link them fails here, at build time.
gmake build config=debug

# use=coverage sets PONY_OUTPUT_SUFFIX to -coverage, so the build output lands in
# build/debug-coverage. Derive it rather than hardcoding (the suffix is
# CMake-determined and could grow more segments in a combined build).
out=$(find build -maxdepth 1 -type d -name 'debug-coverage' | head -1)
if [ -z "$out" ] || [ ! -x "$out/ponyc" ]; then
  echo "FAIL: coverage build output directory not found"
  exit 1
fi

smoke=/tmp/coverage-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    env.out.print("coverage smoke ok")
PONY

# Compile a program and run it — confirms the coverage ponyc produces a working
# binary. That ponyc invocation is real work for the instrumented compiler, so it
# also exercises ponyc's own coverage; capture this run's profile data with a
# per-invocation LLVM_PROFILE_FILE (not exported, so it captures only this run,
# not the many ponyc runs `gmake build` already made).
prof="$smoke/ponyc.profraw"
rm -f "$prof"
PONYPATH="$PWD/packages" LLVM_PROFILE_FILE="$prof" "$out/ponyc" --debug -o "$smoke" "$smoke"
# Run from $smoke so any stray profile file the program might drop (e.g. a clang
# default.profraw, written relative to the cwd) lands in the scratch dir, not the
# source tree. We don't assert on it — this is the build/link/run check.
out_text=$(cd "$smoke" && "$smoke/coverage-smoke")
echo "$out_text"
echo "$out_text" | grep -q "coverage smoke ok"

# The point of use=coverage: the instrumented ponyc emits profile data when it
# runs. The driver-linked ponyc binary carries clang's profile runtime, so a real
# compile drops a non-empty .profraw. A non-empty file proves the runtime is
# linked and ran; an empty/missing one means coverage isn't actually capturing.
if [ ! -s "$prof" ]; then
  echo "FAIL: coverage-instrumented ponyc produced no profile data"
  exit 1
fi
echo "ponyc dropped profile data: $(wc -c < "$prof") bytes"

# The self-hosted tools are themselves Pony programs the coverage ponyc linked
# during `gmake build`; confirm one runs, proving the tool link path works under
# coverage and not just that a minimal program does.
"$out/pony-lint" --version
echo "coverage smoke test passed"
