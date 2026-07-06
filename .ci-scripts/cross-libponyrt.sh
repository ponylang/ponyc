#!/bin/sh
# Cross-compile libponyrt and the CRT objects for a target arch. Invoked from
# the tier-3 cross jobs in
# .github/workflows/ponyc-tier3.yml. The cmake cross machinery already exists
# (PONY_CROSS_LIBPONYRT in the top-level CMakeLists.txt); this just drives it.
#
# Usage: cross-libponyrt.sh <config> <CC> <CXX> <arch> <cflags> <llc_flags>
#   config     debug | release
#   CC/CXX     the cross toolchain (e.g. riscv64-linux-gnu-gcc-10)
#   arch       PONY_ARCH / CMAKE_SYSTEM_PROCESSOR (e.g. rv64gc, armv7-a)
#   cflags     CMAKE_C_FLAGS / CMAKE_CXX_FLAGS for the target
#   llc_flags  LL_FLAGS (cmake ;-list) passed through to llc
#
# The build lands in build/<arch>/build_<config>, output in build/<arch>/<config>,
# which the cross test picks up via PONYPATH.
set -eu

config="$1"
cc="$2"
cxx="$3"
arch="$4"
cflags="$5"
llc_flags="$6"

dir="build/$arch/build_$config"
cmake -B "$dir" -S . \
  -DCMAKE_CROSSCOMPILING=true \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR="$arch" \
  -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_CXX_COMPILER="$cxx" \
  -DPONY_CROSS_LIBPONYRT=true \
  -DCMAKE_BUILD_TYPE="$config" \
  -DCMAKE_C_FLAGS="$cflags" \
  -DCMAKE_CXX_FLAGS="$cflags" \
  -DPONY_ARCH="$arch" \
  -DLL_FLAGS="$llc_flags"
cmake --build "$dir" --config "$config" --target libponyrt crt_objects
