#!/usr/bin/env bash
set -eu
trial=$(cd "$(dirname "$0")" && pwd)
for changed in retained replacement; do
  dir="$trial/controls/$changed"
  mkdir -p "$dir/source"
  if [[ $changed == retained ]]; then
    sed '/^    @probe_check(kept, replacement, n)$/i\    kept.value = n + 1' \
      "$trial/fixture/main.pony" > "$dir/source/main.pony"
  else
    sed 's/Cell(211)/Cell(212)/' "$trial/fixture/main.pony" > "$dir/source/main.pony"
  fi
  for mode in debug optimized; do
    opts=()
    if [[ $mode == debug ]]; then opts=(-d); fi
    mkdir -p "$dir/$mode"
    "$trial/bin/ponyc" "${opts[@]}" --pic --gc-root-audit -o "$dir/$mode" -b measure "$dir/source"
    for setting in off on; do
      log="$dir/$mode/$setting.txt"
      status=0
      PROBE_COLLECTION="$setting" "$dir/$mode/measure" --ponymaxthreads=1 > "$log" 2>&1 || status=$?
      if [[ $status != 2 ]] || ! grep -Fxq "experiment failed: $changed field payload" "$log"; then
        echo "FAIL $mode $setting: expected rejection of $changed field mismatch, exit $status" >&2
        cat "$log" >&2
        exit 1
      fi
      echo "$mode $setting rejected $changed field mismatch"
    done
  done
done
