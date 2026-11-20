#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
python3 "$base/bitcode.py"
t="$base/bitcode"
for mode in debug optimized; do
  opts=()
  [[ $mode != debug ]] || opts=(-d)
  for lto in fat thin; do
    o="$t/$mode-$lto"
    mkdir -p "$o"
    "$t/bin/ponyc" "${opts[@]}" "--$lto-lto" --pic --gc-root-audit -o "$o" -b measure "$t/fixture" 2>&1 | tee "$o/build.log"
    grep -Fxq 'Merging runtime' "$o/build.log"
    for threads in 1 2; do
      for setting in off on; do
        PROBE_COLLECTION="$setting" "$o/measure" --ponymaxthreads="$threads" --ponynoscale --ponytracingcategories=actor_behavior,actor_gc --ponytracingoutput="$o/$threads-$setting.json" > "$o/$threads-$setting.txt"
        python3 "$t/trace.py" "$o/$threads-$setting.json" "$setting"
        echo "$mode $lto $threads $setting: $(tail -1 "$o/$threads-$setting.txt")"
      done
    done
  done
done
