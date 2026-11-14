#!/usr/bin/env bash
#
# Build ponyc inside a Nix environment and verify that the compiled ponyc can
# link Pony programs. Run inside the committed development shell from the
# checkout root. The caller is responsible for restoring the LLVM libs cache
# before invoking this script.
#
# Usage: nix-build-and-test.sh
set -euo pipefail

echo "=== Verify Nix environment ==="
echo "NIX_CC=$NIX_CC"
echo ""
echo "nix-support/dynamic-linker:"
cat "$NIX_CC/nix-support/dynamic-linker" 2>/dev/null || echo "<not found>"
echo ""
echo "nix-support/orig-libc:"
cat "$NIX_CC/nix-support/orig-libc" 2>/dev/null || echo "<not found>"
echo ""
echo "nix-support/cc-ldflags:"
cat "$NIX_CC/nix-support/cc-ldflags" 2>/dev/null || echo "<not found>"
echo ""

echo "=== Build ponyc (debug) ==="
# Nix's clang wrapper doesn't know where gcc's libstdc++.a lives, so use
# gcc/g++ as the C/C++ compilers for the ponyc build itself.
cmake --preset x86-64-debug \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset x86-64-debug

echo ""
echo "=== Run core tests (debug codegen) ==="
PONY_DEBUG=1 ctest --preset x86-64-debug -L ci-core --output-on-failure

echo ""
echo "=== Run stdlib and full-program tests (release codegen) ==="
env -u PONY_DEBUG ctest --preset x86-64-debug \
  -R '^(stdlib|full-programs)$' --output-on-failure

echo ""
echo "=== Build and run tool tests ==="
cmake --build --preset x86-64-debug --target tool-tests
ctest --preset x86-64-debug -L tools --output-on-failure

echo ""
echo "=== Build ponyc (release) ==="
cmake --preset x86-64-release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset x86-64-release

echo ""
echo "=== Check ponyc links Pony programs ==="
program_workdir=$(mktemp -d)
trap 'rm -rf "$program_workdir"' EXIT
mkdir -p "$program_workdir/src/hello" "$program_workdir/bin"
printf 'actor Main\n  new create(env: Env) =>\n    env.out.print("Hello from Nix!")\n' \
  > "$program_workdir/src/hello/main.pony"
./build/debug/ponyc "$program_workdir/src/hello" \
  -b hello -o "$program_workdir/bin/"
"$program_workdir/bin/hello"

interp=$(readelf -l "$program_workdir/bin/hello" \
  | grep 'Requesting program interpreter' | sed 's/.*: \(.*\)]/\1/')
echo "PT_INTERP: $interp"
case "$interp" in
  /nix/store/*)
    echo "OK: dynamic linker is in the Nix store"
    ;;
  *)
    echo "FAIL: dynamic linker is not in the Nix store: $interp" >&2
    exit 1
    ;;
esac

echo ""
echo "=== Check ponyc links a stdlib-using program ==="
mkdir -p "$program_workdir/src/testprog"
cat > "$program_workdir/src/testprog/main.pony" <<'PONY'
use "collections"
use "format"

actor Main
  new create(env: Env) =>
    let m = Map[String, U64]
    m("hello") = 42
    m("world") = 99
    for (k, v) in m.pairs() do
      env.out.print(k + " = " + Format.int[U64](v))
    end
    env.out.print("NixOS linking works!")
PONY
./build/debug/ponyc "$program_workdir/src/testprog" \
  -b testprog -o "$program_workdir/bin/"
"$program_workdir/bin/testprog"

echo ""
echo "=== ALL TESTS PASSED ==="
