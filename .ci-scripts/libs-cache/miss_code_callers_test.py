#!/usr/bin/env python3
"""Self-contained test of the workflows' shell compares against the miss code.

Run: python3 .ci-scripts/libs-cache/miss_code_callers_test.py
Exits 0 if every check passes, 1 otherwise.

Some workflow steps call `exists` directly from shell and compare its exit code
(`$main_rc` / `$branch_rc`) against the miss code. Shell can't import
`common.EXIT_MISS`, so each such step first reads it from the checked-out
scripts into `$miss` with MISS_LINE, and compares against `"$miss"`. A compare
against a numeric literal reads the wrong code once EXIT_MISS changes. If
`$miss` is unset or not a number, `[ ]` fails with an error, which an
`if`/`elif` treats as false. So the miss test must be `-eq "$miss"`, which then
takes the error branch, never `-ne "$miss"`, which would take the miss branch;
and every step must set `$miss` before comparing against it.
"""
import glob
import os
import re
import subprocess
import sys

from common import EXIT_MISS

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WORKFLOWS = os.path.join(REPO, '.github', 'workflows')

MISS_LINE = ("miss=$(cd .ci-scripts/libs-cache && python3 -c "
             "'from common import EXIT_MISS; print(EXIT_MISS)')")

# Workflows known to compare against "$miss", and how many compares each has:
# five platforms (i386, FreeBSD, OpenBSD, DragonFly, NixOS), main and branch
# cache.
KNOWN_CALLERS = {
    'ponyc-tier3.yml': 10,
    'update-lib-cache.yml': 10,
}

RC_COMPARE = re.compile(r'"\$\w*rc" -(?:eq|ne) ("\$miss"|\d+)')
# A step starts at a list item with a key (`- name:`, `- uses:`, `- id:`, ...);
# a job or top-level key also ends the step before it.
STEP_START = re.compile(r'^(\s*- [A-Za-z][\w-]*:|\S| {2}\S)')

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def workflows():
    for path in sorted(glob.glob(os.path.join(WORKFLOWS, '*.yml'))):
        with open(path) as f:
            yield os.path.basename(path), f.read().splitlines()


def test_no_literal_miss_compares():
    for name, lines in workflows():
        literals = [m.group(1) for line in lines
                    for m in RC_COMPARE.finditer(line)
                    if m.group(1) not in ('"$miss"', '0')]
        check(f"{name}, exit-code compares against a literal", literals, [])


def test_no_negated_miss_compares():
    for name, lines in workflows():
        negated = [number for number, line in enumerate(lines, 1)
                   if re.search(r'-ne "\$miss"', line)]
        check(f"{name}, lines comparing -ne \"$miss\"", negated, [])


def test_known_callers_compare_against_miss():
    found = {}
    for name, lines in workflows():
        count = sum(1 for line in lines for m in RC_COMPARE.finditer(line)
                    if m.group(1) == '"$miss"')
        if count:
            found[name] = count
    check('compares against "$miss" per workflow', found, KNOWN_CALLERS)


def test_miss_is_set_in_the_same_step():
    for name, lines in workflows():
        miss_set = False
        for number, line in enumerate(lines, 1):
            if STEP_START.match(line):
                miss_set = False
            if 'miss=$(' in line:
                check(f"{name}:{number}, miss line", line.strip(), MISS_LINE)
                miss_set = True
            if '"$miss"' in line:
                check(f"{name}:{number}, $miss set earlier in the step",
                      miss_set, True)


def test_miss_line_prints_exit_miss():
    proc = subprocess.run(['bash', '-c', MISS_LINE + '; echo "$miss"'],
                          cwd=REPO, capture_output=True, text=True)
    check('miss line exit', proc.returncode, 0)
    check('miss line value', proc.stdout.strip(), str(EXIT_MISS))


TESTS = [test_no_literal_miss_compares, test_no_negated_miss_compares,
         test_known_callers_compare_against_miss,
         test_miss_is_set_in_the_same_step, test_miss_line_prints_exit_miss]


def main():
    for t in TESTS:
        t()
    if FAILURES:
        print(f"FAIL ({len(FAILURES)}):")
        for f in FAILURES:
            print(f"  {f}")
        sys.exit(1)
    print(f"ok ({len(TESTS)} test functions)")


if __name__ == '__main__':
    main()
