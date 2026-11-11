#!/bin/sh
# Compile and run the stdlib test suite one package at a time.
#
# The monolithic stdlib binary links every stdlib package into one test binary,
# which exhausts the ILP32 address space (~3 GB) during LLVM code generation.
# Per-package testing stays well within the limit.
#
# Usage: i386-stdlib-perpkg.sh <preset>
#   preset   the CMake preset the build was configured with (debug | release)
#
# Codegen mode is controlled by the PONY_DEBUG env var (same as ctest):
#   PONY_DEBUG=1  → compile Pony sources with -d (debug codegen)
#   unset/empty   → release codegen
set -eu

preset="$1"

case "$preset" in
    debug)   config=debug   ;;
    release) config=release ;;
    *)
        echo "i386-stdlib-perpkg.sh: unsupported preset '$preset'" >&2
        exit 1
        ;;
esac

ponyc="./build/$config/ponyc"
outdir="./build/$config"
pkgdir="./packages"

# The SSL -D flag the build chose for this preset. CMake picks it from the SSL
# headers it builds against, which can belong to a different SSL library than the
# `openssl` binary on PATH. CMakeLists.txt records the flag in the cache.
cache="./build/build_$preset/CMakeCache.txt"
ssl_flag=$(sed -n 's/^PONY_SSL_FLAG:[A-Z]*=//p' "$cache" 2>/dev/null) || true
if [ -z "$ssl_flag" ]; then
    echo "i386-stdlib-perpkg.sh: no PONY_SSL_FLAG in $cache; configure the '$preset' preset first" >&2
    exit 1
fi

debug_flag=""
codegen_label="release"
if [ "${PONY_DEBUG:-}" = "1" ]; then
    debug_flag="-d"
    codegen_label="debug"
fi

echo "=== per-package stdlib ($config runtime, $codegen_label codegen, $ssl_flag) ==="

# Every package that registers tests in packages/stdlib/_test.pony, listed by
# its filesystem path under packages/.
packages="
actor_pinning
binary_codec
buffered
builtin_test
bureaucracy
byte_order
cli
collections
collections/persistent
constrained_types
crypto
encode/base64
files
format
http_client
ini
iregex
itertools
json
math
net
pony_test
process
promises
random
runtime_info
runtime_tracing
signals
strings
term
time
uri
"

failures=""
count=0
pass=0

for pkg in $packages; do
    count=$((count + 1))
    printf "  %-30s " "$pkg"

    # shellcheck disable=SC2086
    if ! $ponyc $debug_flag -b stdlib_test --checktree $ssl_flag --pic \
          "$pkgdir/$pkg" -o "$outdir" > /tmp/perpkg-compile.log 2>&1; then
        echo "COMPILE FAILED"
        cat /tmp/perpkg-compile.log
        failures="$failures $pkg(compile)"
        continue
    fi

    if ! "$outdir/stdlib_test" --sequential > /tmp/perpkg-run.log 2>&1; then
        echo "TEST FAILED"
        cat /tmp/perpkg-run.log
        failures="$failures $pkg(test)"
        continue
    fi

    echo "ok"
    pass=$((pass + 1))
    rm -f "$outdir/stdlib_test"
done

rm -f /tmp/perpkg-compile.log /tmp/perpkg-run.log

echo ""
echo "$pass/$count packages passed ($config runtime, $codegen_label codegen)"
if [ -n "$failures" ]; then
    echo "FAILURES:$failures"
    exit 1
fi
