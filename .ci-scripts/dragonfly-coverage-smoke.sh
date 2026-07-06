#!/bin/sh
# Coverage smoke test for the DragonFly tier-3 job. Runs inside the
# DragonFly VM, invoked from .github/workflows/ponyc-tier3.yml. POSIX sh (no
# bash in the DragonFly base system). Expects to run from the ponyc source root
# with the gcc13 build env already exported (CC/CXX/LD_LIBRARY_PATH/SSL_CERT_FILE).
#
# use=coverage is for working on ponyc itself: a coverage-instrumented ponyc
# records which parts of the compiler each run exercises. DragonFly builds ponyc
# only with gcc (the -fprofile-arcs path). gcc's -fprofile-arcs instruments
# libponyrt with gcov constructors referencing __gcov_init/__gcov_exit from
# libgcov; ponyc links Pony programs via embedded LLD (not the driver), so libgcov
# is spliced onto the link line (#5434; see the coverage capture in the top-level
# CMakeLists.txt and the PONY_COVERAGE block in genexe.cc) — without it the build
# can't link the self-hosted tools at all.
#
# This smoke guards two things:
#   1. use=coverage still builds and links on gcc — `cmake --build` (which links the
#      self-hosted tools via embedded LLD) is the first assertion; before #5434 it
#      died with undefined __gcov_init. Compiling and running a standalone program
#      then confirms the spliced libgcov satisfies the gcov constructors at
#      runtime, not just at link time.
#   2. the coverage-instrumented ponyc emits profile data when it runs (.gcda) —
#      the point of use=coverage, measuring ponyc's own coverage.
set -eu

# Configure the debug build from scratch with coverage (remove the cmake build dir
# first); the prebuilt LLVM in build/libs is a separate dir and untouched, so it
# is not rebuilt. A from-scratch
# configure is required because the coverage capture and instrumentation flags
# differ from the plain debug build the job already made.
rm -rf build/build_debug
cmake --preset debug -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DPONY_USES=coverage
cmake --build --preset debug

# use=coverage sets PONY_OUTPUT_SUFFIX to -coverage, so the build output lands in
# build/debug-coverage. Derive it rather than hardcoding (the suffix is
# CMake-determined and could grow more segments in a combined build).
out=$(find build -maxdepth 1 -type d -name 'debug-coverage' | head -1)
if [ -z "$out" ] || [ ! -x "$out/ponyc" ]; then
  echo "FAIL: coverage build output directory not found"
  exit 1
fi

smoke=/build/coverage-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    env.out.print("coverage smoke ok")
PONY

# Compile a program (link via embedded LLD — the captured -lgcov fragment is
# spliced here; a broken splice fails at link time with undefined __gcov_init) and
# run it. The same ponyc invocation is real work for the instrumented compiler, so
# capture its .gcda with a per-invocation GCOV_PREFIX (not exported) to assert
# ponyc's own coverage below.
gcov_dir="$smoke/gcov"
rm -rf "$gcov_dir"
mkdir -p "$gcov_dir"
PONYPATH="$PWD/packages" GCOV_PREFIX="$gcov_dir" "$out/ponyc" --debug -o "$smoke" "$smoke"
# Run from $smoke so any stray profile file the program might drop lands in the
# scratch dir, not the source tree. We don't assert on it — this is the
# build/link/run check.
out_text=$(cd "$smoke" && "$smoke/coverage-smoke")
echo "$out_text"
echo "$out_text" | grep -q "coverage smoke ok"

# The point of use=coverage: the instrumented ponyc emits profile data when it
# runs. gcc writes a .gcda per instrumented TU at exit, redirected above into
# $gcov_dir, so a real compile must have written some. Assert at least one landed;
# none means coverage isn't actually capturing data.
gcda_count=$(find "$gcov_dir" -name '*.gcda' | wc -l | tr -dc '0-9')
if [ "$gcda_count" -eq 0 ]; then
  echo "FAIL: coverage-instrumented ponyc produced no .gcda profile data"
  exit 1
fi
echo "ponyc dropped profile data: $gcda_count .gcda files"

# The self-hosted tools are themselves Pony programs the coverage ponyc linked
# during `cmake --build`; confirm one runs, proving the tool link path works under
# coverage and not just that a minimal program does.
"$out/pony-lint" --version
echo "coverage smoke test passed"
