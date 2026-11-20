#!/usr/bin/env bash
set -euo pipefail
out=$(cd "$(dirname "$0")" && pwd)/logs
timeout 1800 cmake --build --preset debug --parallel 2 2>&1 | tee "$out/build.log"
PONY_DEBUG=1 timeout 2400 cmake --build --preset debug --target tool-tests --parallel 1 2>&1 | tee "$out/tool-build.log"
PONY_DEBUG=1 timeout 4800 ctest --preset debug -j1 --output-on-failure 2>&1 | tee "$out/tests.log"
env -u PONY_DEBUG -u PONY_THIN_LTO timeout 4800 ctest --preset debug -R '^(stdlib|full-programs)$' -j1 --output-on-failure 2>&1 | tee "$out/optimized.log"
