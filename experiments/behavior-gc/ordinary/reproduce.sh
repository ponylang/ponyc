#!/usr/bin/env bash
set -euo pipefail
trial=$(cd "$(dirname "$0")" && pwd)
test -f src/libponyrt/actor/actor.c
if [ -e "$trial/measurements.csv" ]; then
  echo "Copy the harness to a fresh directory to record a new run." >&2
  exit 2
fi
if [ "$(git -C "${PEG_DIR:-$HOME/code/ponylang/peg}" rev-parse HEAD)" != 810ff6068018a96486a85a2d46e98ceabdeb3ef5 ]; then
  echo "The PEG source and golden file require commit 810ff6068018a96486a85a2d46e98ceabdeb3ef5." >&2
  exit 2
fi
python3 "$trial/build.py"
python3 "$trial/fixtures.py"
cc -O2 -Wall -Werror "$trial/time-child.c" -o "$trial/time-child"
python3 "$trial/run.py"
python3 "$trial/phase.py"
python3 "$trial/controls.py"
python3 "$trial/refusal.py"
python3 "$trial/cost.py"
python3 "$trial/internal_cost.py"
python3 "$trial/oracle.py"
