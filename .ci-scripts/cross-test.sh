#!/bin/sh
# Cross-compilation test for the tier-3 cross jobs. Invoked from
# .github/workflows/ponyc-tier3.yml. Runs on the x86-64 host with a host ponyc
# and a cross libponyrt built by cross-libponyrt.sh.
#
# It does two things:
#   1. The host ponyc's own suites run natively -- the gtest suites, full-program
#      tests, examples, grammar (everything in ci-core except the stdlib, which we
#      cross-build below). This is `ctest -L ci-core -E stdlib`.
#   2. The stdlib is cross-compiled with the host ponyc pointed at the cross
#      libponyrt (PONYPATH) and run under the emulator, for both configs.
#
# Usage: cross-test.sh <preset> <cross_ponypath> <cross_args> <cross_runner> <stdlib_excludes>
#   preset          the CMake preset the host ponyc was configured with
#   cross_ponypath  cross libponyrt dir, relative to ponyc's output directory
#                   (e.g. ../rv64gc/debug)
#   cross_args      ponyc cross flags: --triple=.. --cpu=.. --link-arch=.. [--sysroot=..] <extra>
#   cross_runner    the emulator command (e.g. "qemu-riscv64 -L /usr/riscv64-linux-gnu/lib/")
#   stdlib_excludes stdlib test excludes (e.g. --exclude=net/)
set -eu

preset="$1"
cross_ponypath="$2"
cross_args="$3"
cross_runner="$4"
stdlib_excludes="$5"

# ponyc's output directory is named after the build type, not after the preset.
# Map rather than strip a suffix: a preset that sets PONY_USES lands in a
# directory with those options appended (BUILD.md).
case "$preset" in
    debug|x86-64-debug) config=debug ;;
    release|x86-64-release) config=release ;;
    *)
        echo "cross-test.sh: unsupported preset '$preset'" >&2
        exit 1
        ;;
esac

# 1. Host-native tests: all of ci-core except the stdlib (cross-built below).
ctest --preset "$preset" -L ci-core -E stdlib

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
