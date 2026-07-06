#!/bin/sh
# Coverage smoke test for the Linux weekly-checks job. Invoked from
# .github/workflows/ponyc-weekly-checks.yml. Expects to run from the ponyc source
# root, in a container that has built libs, with CC/CXX selecting the toolchain to
# exercise (gcc or clang — the weekly job runs it once with each).
#
# use=coverage is for working on ponyc itself: a coverage-instrumented ponyc
# records which parts of the compiler each run exercises. The instrumentation and
# the data files differ by toolchain:
#   - gcc (-fprofile-arcs): data lands as .gcda files, one per instrumented TU,
#     redirected by GCOV_PREFIX to a scratch dir. gcc instruments libponyrt with
#     gcov constructors referencing __gcov_init/__gcov_exit from libgcov; ponyc
#     links Pony programs via embedded LLD (not the driver), so libgcov is spliced
#     onto the link line (#5434; see the coverage capture in the top-level
#     CMakeLists.txt and the PONY_COVERAGE block in genexe.cc) — without it the
#     build can't link the self-hosted tools.
#   - clang (-fprofile-instr-generate): data lands as a single .profraw selected by
#     LLVM_PROFILE_FILE.
#
# This guards, for whichever toolchain CC/CXX selects, two things:
#   1. use=coverage builds and links the Pony programs ponyc compiles (the
#      self-hosted tools during the ponyc build (`cmake --build`), plus the
#      standalone program below) via embedded LLD.
#   2. the coverage-instrumented ponyc emits profile data when it runs — the point
#      of use=coverage, measuring ponyc's own coverage.
set -eu

# CC/CXX must be set — they select the toolchain to exercise, and the coverage
# data format (clang .profraw vs gcc .gcda) is chosen below from CXX, so it has
# to match the compiler the build actually used. Fail loudly if unset rather than
# defaulting and risking a format mismatch (e.g. the presets prefer clang when
# CC/CXX are unset, which would disagree with a gcc-style assertion).
: "${CC:?set CC and CXX (gcc/g++ or clang/clang++)}"
: "${CXX:?set CC and CXX (gcc/g++ or clang/clang++)}"

# Configure the debug build from scratch with coverage. Remove the cmake build
# dir first so the reconfigure is clean; the prebuilt LLVM in build/libs is a
# separate dir and is untouched, so this does not rebuild LLVM. Override the
# preset's pinned clang with the CC/CXX this run exercises (the base preset pins
# clang; coverage runs once per toolchain and the data format follows CXX).
rm -rf build/build_debug
cmake --preset debug -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DPONY_USES=coverage
# The `cmake --build` below is itself the first assertion: it compiles the
# instrumented ponyc and links the self-hosted tools (pony-lsp/pony-lint/
# pony-doc), Pony programs ponyc links via embedded LLD.
cmake --build --preset debug

# use=coverage sets PONY_OUTPUT_SUFFIX to -coverage. Derive the output dir.
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

# The coverage data format follows the toolchain libponyrt was instrumented with,
# which CMake selects from the C++ compiler id (clang -> profraw, else gcov). Pick
# the matching capture/assertion path from the same compiler CC/CXX selected. The
# program compile below links via embedded LLD (on gcc the spliced libgcov is what
# lets it link); running it confirms the coverage ponyc produces a working binary,
# and the same compile is real work for the instrumented compiler, so it also
# exercises ponyc's own coverage — captured per-invocation and asserted below.
if "$CXX" --version 2>/dev/null | grep -qi clang; then
  # clang: LLVM_PROFILE_FILE selects a per-invocation .profraw, asserted non-empty.
  ponyc_data="$smoke/ponyc.profraw"
  rm -f "$ponyc_data"
  PONYPATH="$PWD/packages" LLVM_PROFILE_FILE="$ponyc_data" "$out/ponyc" --debug -o "$smoke" "$smoke"
  # Run from $smoke so a stray clang default.profraw lands in scratch, not the tree.
  out_text=$(cd "$smoke" && "$smoke/coverage-smoke")
  echo "$out_text"
  echo "$out_text" | grep -q "coverage smoke ok"
  if [ ! -s "$ponyc_data" ]; then
    echo "FAIL: coverage-instrumented ponyc produced no profile data"
    exit 1
  fi
  echo "ponyc dropped profile data: $(wc -c < "$ponyc_data") bytes"
else
  # gcc: GCOV_PREFIX redirects the per-TU .gcda files; assert at least one lands.
  ponyc_gcov="$smoke/gcov-ponyc"
  rm -rf "$ponyc_gcov"
  mkdir -p "$ponyc_gcov"
  PONYPATH="$PWD/packages" GCOV_PREFIX="$ponyc_gcov" "$out/ponyc" --debug -o "$smoke" "$smoke"
  # Run from $smoke so any stray profile file lands in scratch, not the tree.
  out_text=$(cd "$smoke" && "$smoke/coverage-smoke")
  echo "$out_text"
  echo "$out_text" | grep -q "coverage smoke ok"
  ponyc_gcda=$(find "$ponyc_gcov" -name '*.gcda' | wc -l | tr -dc '0-9')
  if [ "$ponyc_gcda" -eq 0 ]; then
    echo "FAIL: coverage-instrumented ponyc produced no .gcda profile data"
    exit 1
  fi
  echo "ponyc dropped profile data: $ponyc_gcda .gcda files"
fi

# The self-hosted tools are themselves Pony programs the coverage ponyc linked
# during the ponyc build (`cmake --build`); confirm one runs, proving the tool
# link path works under coverage and not just that a minimal program does.
"$out/pony-lint" --version
echo "coverage smoke test passed"
