#!/bin/sh
# ThinLTO smoke test for the weekly-checks job. Invoked from
# .github/workflows/ponyc-weekly-checks.yml. Expects to run from the ponyc
# source root after `cmake --build --preset debug` (or equivalent) has
# produced a working ponyc.
#
# Compiles and runs a minimal program with --thin-lto to catch regressions
# in the ThinLTO bitcode-emission path (e.g. missing GUID metadata causing
# LLVM fatal errors during summary writing).
set -eu

out="build/debug"
ponyc="$out/ponyc"

if [ ! -x "$ponyc" ]; then
  echo "FAIL: ponyc not found at $ponyc"
  exit 1
fi

smoke=/tmp/thin-lto-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    env.out.print("thin-lto smoke ok")
PONY

echo "--- thin-lto (release) ---"
PONYPATH="$PWD/packages" "$ponyc" --thin-lto -o "$smoke" -b thin-lto-release "$smoke"
result=$("$smoke/thin-lto-release")
echo "$result"
echo "$result" | grep -q "thin-lto smoke ok"

echo "--- thin-lto --debug ---"
PONYPATH="$PWD/packages" "$ponyc" --thin-lto --debug -o "$smoke" -b thin-lto-debug "$smoke"
result=$("$smoke/thin-lto-debug")
echo "$result"
echo "$result" | grep -q "thin-lto smoke ok"

rm -rf "$smoke"
echo "thin-lto smoke test passed"
