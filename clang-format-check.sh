#!/bin/bash
#
# Runs clang-format --dry-run against all C/C++ files in src/ and reports
# per-file violation counts. Use this to measure the effect of .clang-format
# changes against a saved baseline.
#
# Usage:
#   ./clang-format-check.sh              # print report to stdout
#   ./clang-format-check.sh > report.txt # save to file
#   diff <(./clang-format-check.sh) clang-format-baseline.txt  # compare to baseline
#
# Requires: clang-format

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$REPO_ROOT/src"

echo "# clang-format violation report"
echo "# clang-format version: $(clang-format --version)"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# config: $REPO_ROOT/.clang-format"
echo "#"
echo "# violations file"

total_violations=0
total_files=0
failing_files=0

while IFS= read -r -d '' f; do
  total_files=$((total_files + 1))
  short="${f#$REPO_ROOT/}"
  clang-format --dry-run --Werror "$f" > /tmp/cf_check_tmp.txt 2>&1 || true
  count=$(grep -c ': error:' /tmp/cf_check_tmp.txt || true)
  if [ "$count" -gt 0 ]; then
    failing_files=$((failing_files + 1))
    total_violations=$((total_violations + count))
  fi
  echo "$count $short"
done < <(find "$SRC_DIR" -type f \( -name '*.c' -o -name '*.h' -o -name '*.cc' \) -print0 | sort -z)

rm -f /tmp/cf_check_tmp.txt

echo "#"
echo "# total_files: $total_files"
echo "# failing_files: $failing_files"
echo "# total_violations: $total_violations"
