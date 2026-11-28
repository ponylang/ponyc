#!/bin/sh
# Coverage smoke test for the FreeBSD tier-3 job. Runs inside the FreeBSD VM,
# invoked from .github/workflows/ponyc-tier3.yml. POSIX sh, not bash: FreeBSD's
# base system has no bash. Expects to run from the ponyc source root.
#
# use=coverage is for working on ponyc itself: a coverage-instrumented ponyc
# records which parts of the compiler each run exercises. FreeBSD builds ponyc
# with its base clang (the -fprofile-instr-generate path). This smoke guards two
# things:
#   1. use=coverage still builds — it compiles the instrumented ponyc and
#      ponyc-compiler and links the self-hosted tools (pony-lsp/pony-lint/
#      pony-doc) plus a standalone program, so a coverage build that can't link
#      is caught here.
#   2. the coverage-instrumented compiler (ponyc-compiler) actually emits profile
#      data when it runs (a .profraw) — the point of use=coverage, measuring the
#      compiler's own coverage.
set -eu

# Configure the debug build from scratch with coverage (remove the cmake build dir
# first); the prebuilt LLVM in build/libs is a separate dir and untouched, so it
# is not rebuilt. A from-scratch configure is required because the coverage
# instrumentation flags differ from the plain debug build the job already made.
# This runs after the sanitizer smoke, which performs the same clean+reconfigure.
rm -rf build/build_x86-64-debug
cmake --preset x86-64-debug -DPONY_USES=coverage
# The `cmake --build` below is itself the first assertion: it compiles the
# instrumented ponyc and links the self-hosted tools (pony-lsp/pony-lint/
# pony-doc). A coverage build that can't link them fails here, at build time.
cmake --build --preset x86-64-debug

# use=coverage sets PONY_OUTPUT_SUFFIX to -coverage, so the build output lands in
# build/debug-coverage. Derive it rather than hardcoding (the suffix is
# CMake-determined and could grow more segments in a combined build).
out=$(find build -maxdepth 1 -type d -name 'debug-coverage' | head -1)
if [ -z "$out" ] || [ ! -x "$out/ponyc" ] || [ ! -x "$out/ponyc-compiler" ]; then
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

# Compile a program through ponyc, which hands the final link to ponylink, and
# run it — confirms the coverage build produces a working binary.
# Run from $smoke so any stray profile file the program might drop (e.g. a clang
# default.profraw, written relative to the cwd) lands in the scratch dir, not the
# source tree. We don't assert on it — this is the build/link/run check.
PONYPATH="$PWD/packages" "$out/ponyc" --debug -o "$smoke/via-ponyc" "$smoke"
out_text=$(cd "$smoke" && "$smoke/via-ponyc/coverage-smoke")
echo "$out_text"
echo "$out_text" | grep -q "coverage smoke ok"

# Run the same compile with ponyc-compiler directly, so the profile data
# checked comes only from the compiler: ponyc is instrumented too, and its data
# alone would satisfy the check. LLVM_PROFILE_FILE is set for this run only.
prof="$smoke/ponyc.profraw"
rm -f "$prof"
PONYPATH="$PWD/packages" LLVM_PROFILE_FILE="$prof" "$out/ponyc-compiler" --debug -o "$smoke" "$smoke"
if [ ! -s "$prof" ]; then
  echo "FAIL: coverage-instrumented ponyc-compiler produced no profile data"
  exit 1
fi
echo "ponyc-compiler dropped profile data: $(wc -c < "$prof") bytes"

# The self-hosted tools are themselves Pony programs the coverage ponyc linked
# during `cmake --build`; confirm one runs, proving the tool link path works under
# coverage and not just that a minimal program does.
"$out/pony-lint" --version
echo "coverage smoke test passed"
