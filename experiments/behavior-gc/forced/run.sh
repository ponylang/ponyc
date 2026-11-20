#!/usr/bin/env bash
set -eu
trial=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$trial/logs"
python3 "$trial/build.py"
for mode in debug optimized; do
  opts=()
  if [[ $mode == debug ]]; then opts=(-d); fi
  mkdir -p "$trial/$mode"
  "$trial/bin/ponyc" "${opts[@]}" --pic --gc-root-audit -o "$trial/$mode" -b measure "$trial/fixture"
  for setting in off on; do
    rm -f -- "$trial/logs/$mode-$setting.json"
    PROBE_COLLECTION="$setting" "$trial/$mode/measure" --ponymaxthreads=1 --ponytracingcategories=actor_behavior,actor_gc --ponytracingoutput="$trial/logs/$mode-$setting.json" > "$trial/logs/$mode-$setting.txt"
    cat "$trial/logs/$mode-$setting.txt"
    python3 "$trial/trace.py" "$trial/logs/$mode-$setting.json" "$setting"
  done
done
bash "$trial/payload-controls.sh"
python3 "$trial/trace-controls.py"
