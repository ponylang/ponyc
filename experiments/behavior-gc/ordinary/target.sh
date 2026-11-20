#!/bin/sh
set -eu
mkdir -p objects
case "$(cc -dumpmachine)" in
  i?86-*) cpu='-march=pentium4' ;;
  arm*) cpu='-march=native' ;;
  *) exit 2 ;;
esac
stage=$1
set -- "$cpu" -O1 -std=gnu11 -fPIC -fexceptions -DDEBUG -D_FILE_OFFSET_BITS=64 -I. -Isrc/libponyrt -Isrc/common
case "$stage" in
  runtime)
    cc "$@" abi.c -o abi
    ./abi > abi.txt
    cc "$@" -DPONY_WANT_ATOMIC_DEFS abi.c -o abi-atomics
    ./abi-atomics > abi-atomics.txt
    cmp abi.txt abi-atomics.txt
    cat abi.txt
    while read -r name; do
      cc "$@" '-DPONY_BUILD_CONFIG="debug"' -c "src/libponyrt/$name" -o "objects/$(echo "$name" | tr / _).o"
    done < sources
    ar rcs libponyrt.a objects/*.o ;;
  programs)
    for obj in work-*.o; do
      name=${obj%.o}
      c++ "$cpu" -o "$name" "$obj" libponyrt.a -pthread -ldl -lm -latomic
      readelf -h "$name" > "$name.elf"
      "./$name" large --ponymaxthreads=1 --ponytracingcategories=actor_behavior,actor_gc "--ponytracingoutput=$name.json" > "$name.out"
    done ;;
esac
