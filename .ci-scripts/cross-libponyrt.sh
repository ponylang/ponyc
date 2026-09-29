#!/bin/sh
# Cross-compile libponyrt, the CRT objects, and libponyrt.bc for a target arch.
# Invoked from .github/workflows/ponyc-tier3.yml. The cmake cross machinery
# already exists (PONY_CROSS_LIBPONYRT in the top-level CMakeLists.txt); this
# just drives it.
#
# Usage: cross-libponyrt.sh <config> <CC> <CXX> <arch> <cflags> <triple>
#   config     debug | release
#   CC/CXX     the cross toolchain (must be Clang for bitcode generation)
#   arch       PONY_ARCH / CMAKE_SYSTEM_PROCESSOR (e.g. rv64gc, armv7-a)
#   cflags     CMAKE_C_FLAGS / CMAKE_CXX_FLAGS for the target
#   triple     the target triple (e.g. riscv64-unknown-linux-gnu); sets
#              CMAKE_C_COMPILER_TARGET so Clang cross-compiles and the
#              bitcode path uses the right --target=
#
# The build lands in build/<arch>/build_<config>, output in build/<arch>/<config>,
# which the cross test picks up via PONYPATH.
set -eu

config="$1"
cc="$2"
cxx="$3"
arch="$4"
cflags="$5"
triple="$6"

# LLVM_VERSION is needed by the bitcode compile commands (-DLLVM_VERSION=...).
# Extract it from the host build's CMake cache; the host must be configured
# before this script runs (the workflow builds the host ponyc first).
llvm_version=$(sed -n 's/^LLVM_PACKAGE_VERSION:.*=//p' build/build_debug/CMakeCache.txt 2>/dev/null \
            || sed -n 's/^LLVM_PACKAGE_VERSION:.*=//p' build/build_release/CMakeCache.txt 2>/dev/null \
            || echo "")

dir="build/$arch/build_$config"
cmake -B "$dir" -S . \
  -DCMAKE_CROSSCOMPILING=true \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR="$arch" \
  -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_CXX_COMPILER="$cxx" \
  -DCMAKE_C_COMPILER_TARGET="$triple" \
  -DCMAKE_CXX_COMPILER_TARGET="$triple" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DPONY_CROSS_LIBPONYRT=true \
  -DCMAKE_BUILD_TYPE="$config" \
  -DCMAKE_C_FLAGS="$cflags" \
  -DCMAKE_CXX_FLAGS="$cflags" \
  -DPONY_ARCH="$arch" \
  -DLLVM_VERSION="$llvm_version"
cmake --build "$dir" --config "$config" --target libponyrt crt_objects libponyrt_bc
