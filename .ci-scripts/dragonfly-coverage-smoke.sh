#!/bin/sh
# Coverage link smoke test for the DragonFly tier-3 job. Runs inside the
# DragonFly VM, invoked from .github/workflows/ponyc-tier3.yml. POSIX sh (no
# bash in the DragonFly base system). Expects to run from the ponyc source root
# with the gcc13 build env already exported (CC/CXX/LD_LIBRARY_PATH/SSL_CERT_FILE).
#
# DragonFly only builds ponyc with gcc, so it is the platform where the gcc
# coverage path (-fprofile-arcs) is exercised by default. That path was broken
# (#5434): -fprofile-arcs instruments libponyrt with gcov constructors that
# reference __gcov_init/__gcov_exit from libgcov, but ponyc links Pony programs
# via embedded LLD (not the compiler driver), so libgcov never reached the link
# line and every Pony program failed to link. The fix captures the driver's
# coverage link fragment (-lgcov) at configure time and splices it into the
# embedded LLD link (see the coverage capture in the top-level CMakeLists.txt and
# the PONY_COVERAGE block in genexe.cc). This test is the primary guard for that
# fix: a coverage ponyc that can't link is caught here, at build + link time.
#
# The `gmake build` below is itself the first assertion: before the fix it dies
# linking the self-hosted tools / full-program-runner with undefined __gcov_init.
# Then we link AND run a standalone Pony program: running proves the spliced
# libgcov actually satisfies the gcov init/fini constructors at runtime, not just
# that the symbols resolved at link time.
set -eu

# Clean + reconfigure debug from scratch with coverage. `clean` is config-scoped
# (pass config=debug so it removes the debug build/output dirs); the prebuilt
# LLVM in build/libs is untouched, so this does not rebuild LLVM. A from-scratch
# configure is required because the coverage capture and instrumentation flags
# differ from the plain debug build the job already made.
gmake clean config=debug
gmake configure config=debug use=coverage
gmake build config=debug

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

# Link via embedded LLD (the captured -lgcov fragment is spliced here) and run.
# A broken splice fails at link time with undefined __gcov_init/__gcov_exit.
PONYPATH="$PWD/packages" "$out/ponyc" --debug -o "$smoke" "$smoke"
out_text=$("$smoke/coverage-smoke")
echo "$out_text"
echo "$out_text" | grep -q "coverage smoke ok"

# The self-hosted tools are themselves Pony programs the coverage ponyc linked
# during `gmake build`; confirm one runs, proving the tool link path works under
# coverage and not just that a minimal program does.
"$out/pony-lint" --version
echo "coverage link smoke test passed"
