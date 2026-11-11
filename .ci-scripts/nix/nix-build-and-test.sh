#!/usr/bin/env bash
#
# Build ponyc inside a Nix environment and verify that the compiled ponyc can
# link Pony programs. Run inside nixos/nix with nix-shell providing the build
# dependencies. The caller is responsible for restoring the LLVM libs cache
# before invoking this script.
#
# Usage: nix-build-and-test.sh
#
# Expects the ponyc source at /work (a writable copy, not the read-only mount).
set -euo pipefail

cd /work

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
echo "=== Run compiler unit tests ==="
ctest --preset x86-64-debug -R libponyc.tests

echo ""
echo "=== Run runtime unit tests ==="
ctest --preset x86-64-debug -R libponyrt.tests

echo ""
echo "=== Run stdlib tests (debug codegen) ==="
PONY_DEBUG=1 ctest --preset x86-64-debug -R "^stdlib$"

echo ""
echo "=== Run full-program tests (debug codegen) ==="
PONY_DEBUG=1 ctest --preset x86-64-debug -R "^full-programs$"

echo ""
echo "=== Run stdlib tests (release codegen) ==="
ctest --preset x86-64-debug -R "^stdlib$"

echo ""
echo "=== Run full-program tests (release codegen) ==="
ctest --preset x86-64-debug -R "^full-programs$"

echo ""
echo "=== Check ponyc links Pony programs ==="
# The test suites exercise ponyc within the build tree. This step verifies
# that ponyc's embedded linker can find the Nix CRT objects (crt1.o, crti.o,
# crtn.o), glibc, gcc runtime libs, and sets PT_INTERP to the Nix store's
# dynamic linker — the whole point of the NixOS linking support.

mkdir -p /tmp/src/hello /tmp/bin
printf 'actor Main\n  new create(env: Env) =>\n    env.out.print("Hello from Nix!")\n' \
  > /tmp/src/hello/main.pony
./build/debug/ponyc /tmp/src/hello -b hello -o /tmp/bin/
/tmp/bin/hello

interp=$(readelf -l /tmp/bin/hello | grep 'Requesting program interpreter' | sed 's/.*: \(.*\)]/\1/')
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
mkdir -p /tmp/src/testprog
cat > /tmp/src/testprog/main.pony <<'PONY'
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
./build/debug/ponyc /tmp/src/testprog -b testprog -o /tmp/bin/
/tmp/bin/testprog

echo ""
echo "=== ALL TESTS PASSED ==="
