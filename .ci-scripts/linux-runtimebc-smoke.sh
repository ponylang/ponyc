#!/bin/sh
# Runtime-bitcode smoke test for the weekly-checks job. Invoked from
# .github/workflows/ponyc-weekly-checks.yml. Expects to run from the ponyc
# source root, in a container that has already built libs.
#
# Exercises three things:
#   1. PONY_RUNTIME_BITCODE=true produces libponyrt.bc (the llvm-link step).
#   2. A Pony program compiled with --runtimebc links and runs.
#   3. The stdlib test suite compiled with --runtimebc passes.
set -eu

# Build ponyc with the bitcode runtime. Remove any prior cmake build dir so the
# configure is clean; the prebuilt LLVM in build/libs is a separate dir and is
# untouched.
rm -rf build/build_release
cmake --preset release -DPONY_RUNTIME_BITCODE=true
cmake --build --preset release

# Verify the bitcode file was produced.
if [ ! -f build/release/libponyrt.bc ]; then
  echo "FAIL: PONY_RUNTIME_BITCODE=true did not produce libponyrt.bc"
  exit 1
fi
echo "libponyrt.bc built: $(wc -c < build/release/libponyrt.bc) bytes"

ponyc=build/release/ponyc

# The stdlib's net package requires an SSL version define.
ssl_version_str=$(openssl version 2>/dev/null) || true
case "$ssl_version_str" in
    LibreSSL*) ssl_flag="-Dlibressl" ;;
    "OpenSSL 4"*|"OpenSSL  4"*) ssl_flag="-Dopenssl_4.0.x" ;;
    "OpenSSL 3"*|"OpenSSL  3"*) ssl_flag="-Dopenssl_3.0.x" ;;
    "OpenSSL 1.1"*|"OpenSSL  1.1"*) ssl_flag="-Dopenssl_1.1.x" ;;
    *) echo "linux-runtimebc-smoke.sh: cannot detect SSL version from: $ssl_version_str" >&2; exit 1 ;;
esac

# Compile and run a trivial program with --runtimebc.
smoke=/tmp/runtimebc-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
class _Disposable
  fun _final() => None

actor Main
  new create(env: Env) =>
    _Disposable
    env.out.print("runtimebc smoke ok")
PONY

PONYPATH="$PWD/packages" "$ponyc" --runtimebc -b runtimebc-smoke -o "$smoke" "$smoke"
out_text=$("$smoke/runtimebc-smoke")
echo "$out_text"
echo "$out_text" | grep -q "runtimebc smoke ok"

# Compile and run the stdlib test suite with --runtimebc.
PONYPATH=build/release "$ponyc" --runtimebc --pic "$ssl_flag" -b stdlib-runtimebc -o build/release packages/stdlib
build/release/stdlib-runtimebc --sequential

echo "runtimebc smoke test passed"
