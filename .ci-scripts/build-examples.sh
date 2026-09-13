#!/bin/sh
#
# Compile every example package under examples/. Directories whose path
# contains "ffi-" are skipped (they need C libraries the runner may not have).
#
# Usage: build-examples.sh <ponyc> <ssl-flag>
#
#   ponyc     Path to the ponyc binary.
#   ssl-flag  The SSL define flag (e.g. -Dlibressl, -Dopenssl_3.0.x).

set -o errexit
set -o nounset

PONYC="$1"
SSL_FLAG="$2"

failed=""
for dir in $(find examples -name '*.pony' | sed 's|/[^/]*$||' | sort -u); do
  [ "$dir" = "examples" ] && continue
  echo "$dir" | grep -q 'ffi-' && continue

  echo "--- $dir ---"
  if "$PONYC" -d -s --checktree "$SSL_FLAG" -o "$dir" "$dir"; then
    :
  else
    failed="$failed $dir"
  fi
done

if [ -n "$failed" ]; then
  printf '\nFailed examples:%s\n' "$failed"
  exit 1
fi
