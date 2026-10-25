#!/bin/bash
#
# Compare optimization benchmark performance between two ponyc builds.
#
# Usage: ./run.sh <ponyc-A> <ponyc-B> [runs]
#
# <ponyc-A> and <ponyc-B> are paths to ponyc binaries (e.g. build/release/ponyc).
# Compiles each benchmark with both builds, then runs each [runs] times
# (default 7) and reports median wall-clock time.
#
# All benchmarks run with --ponymaxthreads=1 --ponynoyield (single actor, no
# scheduler yielding). Runs are interleaved A/B to reduce thermal bias.
#
# Heap-vs-stack promotion counts are reported by HeapToStack in lld's LTO
# pipeline via LLVM STATISTIC counters (visible with LLVM debug builds).

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

[[ $# -ge 2 ]] || die "usage: $0 <ponyc-A> <ponyc-B> [runs]"

PONYC_A="$(realpath "$1")"
PONYC_B="$(realpath "$2")"
RUNS="${3:-7}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

[[ -x "$PONYC_A" ]] || die "not executable: $PONYC_A"
[[ -x "$PONYC_B" ]] || die "not executable: $PONYC_B"

BENCHMARKS=(heap2stack inline-heap2stack compute vec-math cross-package message-send)

echo "A: $PONYC_A"
echo "B: $PONYC_B"
echo "Runs: $RUNS"
echo ""

# ponyc defaults to release mode (no -d flag needed).
echo "Compiling..."
for bench in "${BENCHMARKS[@]}"; do
  "$PONYC_A" -b "a-${bench}" --output "$WORK" \
    "$SCRIPT_DIR/$bench" > /dev/null 2>&1 \
    || die "A failed to compile $bench"

  "$PONYC_B" -b "b-${bench}" --output "$WORK" \
    "$SCRIPT_DIR/$bench" > /dev/null 2>&1 \
    || die "B failed to compile $bench"
done
echo "Done."
echo ""

median() {
  sort -n | awk -v n="$1" 'NR == int(n/2)+1 { printf "%.3f", $0 }'
}

minmax() {
  sort -n | awk 'NR==1 { mn=$0 } { mx=$0 } END { printf "%.3f-%.3f", mn, mx }'
}

# Time a single run. Uses /usr/bin/time if available, falls back to bash.
time_one() {
  local bin="$1"
  if [[ -x /usr/bin/time ]]; then
    { /usr/bin/time -f '%e' "$bin" \
      --ponymaxthreads=1 --ponynoyield > /dev/null; } 2>&1
  else
    local TIMEFORMAT='%3R'
    { time "$bin" --ponymaxthreads=1 --ponynoyield > /dev/null; } 2>&1
  fi
}

printf "%-32s %10s %14s %10s %14s %10s\n" \
  "Benchmark" "A (med)" "A (range)" "B (med)" "B (range)" "Delta"
printf "%-32s %10s %14s %10s %14s %10s\n" \
  "--------------------------------" "----------" "--------------" \
  "----------" "--------------" "----------"

for bench in "${BENCHMARKS[@]}"; do
  bin_a="$WORK/a-${bench}"
  bin_b="$WORK/b-${bench}"

  # warmup (fail early if a binary crashes)
  "$bin_a" --ponymaxthreads=1 --ponynoyield > /dev/null 2>&1 \
    || die "warmup crashed: $bin_a"
  "$bin_b" --ponymaxthreads=1 --ponynoyield > /dev/null 2>&1 \
    || die "warmup crashed: $bin_b"

  times_a=""
  times_b=""
  for ((r = 1; r <= RUNS; r++)); do
    t=$(time_one "$bin_a")
    times_a+="${t}"$'\n'

    t=$(time_one "$bin_b")
    times_b+="${t}"$'\n'
  done

  med_a=$(printf '%s' "$times_a" | grep -v '^$' | median "$RUNS")
  med_b=$(printf '%s' "$times_b" | grep -v '^$' | median "$RUNS")
  rng_a=$(printf '%s' "$times_a" | grep -v '^$' | minmax)
  rng_b=$(printf '%s' "$times_b" | grep -v '^$' | minmax)

  delta=$(awk "BEGIN {
    if ($med_a > 0)
      printf \"%+.1f%%\", (($med_b - $med_a) / $med_a) * 100
    else
      print \"n/a\"
  }")

  printf "%-32s %9ss %13ss %9ss %13ss %10s\n" \
    "$bench" "$med_a" "$rng_a" "$med_b" "$rng_b" "$delta"
done
