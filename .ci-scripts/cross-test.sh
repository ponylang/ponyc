#!/bin/sh
# Cross-compilation test for the tier-3 cross jobs. Invoked from
# .github/workflows/ponyc-tier3.yml. Runs on the x86-64 host with a
# host ponyc built natively and a cross libponyrt built by cross-libponyrt.sh.
#
# It does two things:
#   1. The host ponyc's own suites run natively -- the gtest suites, full-program
#      tests, examples, grammar (everything in ci-core except the stdlib, which we
#      cross-build below). This is `ctest -L ci-core -E stdlib`.
#   2. The stdlib is cross-compiled with the host ponyc pointed at the cross
#      libponyrt (PONYPATH) and run under the emulator, for both configs.
#
# Usage: cross-test.sh <config> <cross_ponypath> <cross_args> <cross_runner> <stdlib_excludes>
#   config          debug | release (selects which host ponyc runs the tests)
#   cross_ponypath  cross libponyrt dir, relative to build/<config> (e.g. ../rv64gc/debug)
#   cross_args      ponyc cross flags: --triple=.. --cpu=.. --link-arch=.. [--sysroot=..] <extra>
#   cross_runner    the emulator command (e.g. "qemu-riscv64 -L /usr/riscv64-linux-gnu/lib/")
#   stdlib_excludes stdlib test excludes (e.g. --exclude=net/)
set -eu

config="$1"
cross_ponypath="$2"
cross_args="$3"
cross_runner="$4"
stdlib_excludes="$5"

# 1. Host-native tests: all of ci-core except the stdlib (cross-built below).
ctest --preset "$config" -L ci-core -E stdlib

# 2. Cross-compile the stdlib with the host ponyc + the cross libponyrt, run it
# under the emulator. Both configs. cross_args,
# cross_runner, and stdlib_excludes must word-split into multiple arguments.
cd "build/$config"
# shellcheck disable=SC2086
PONYPATH=".:$cross_ponypath" ./ponyc -b stdlib-release --pic --checktree $cross_args ../../packages/stdlib
echo "Built $(pwd)/stdlib-release"
# shellcheck disable=SC2086
$cross_runner ./stdlib-release --sequential $stdlib_excludes
# shellcheck disable=SC2086
PONYPATH=".:$cross_ponypath" ./ponyc -d -b stdlib-debug --pic --strip --checktree $cross_args ../../packages/stdlib
echo "Built $(pwd)/stdlib-debug"
# shellcheck disable=SC2086
$cross_runner ./stdlib-debug --sequential $stdlib_excludes
