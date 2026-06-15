#!/usr/bin/env python3
"""Self-contained test that registry.repo_root() points at the actual repo root.

Run: python3 .ci-scripts/libs-cache/repo_root_test.py
Exits 0 if every check passes, 1 otherwise.

registry.py locates `build/libs` relative to repo_root() (a __file__-based
dirname walk) for build_archive/extract_archive. Moving the libs-cache scripts to
a different directory depth silently breaks that walk -- push looks for build/libs
in the wrong place, pull extracts there. This pins the depth so such a move fails
here instead of in CI.
"""
import os
import sys

import registry

FAILURES = []

# A directory is the repo root iff it contains these markers (the build files
# repo_root() exists to find build/libs alongside).
MARKERS = ['Makefile', 'lib', 'src']


def test_repo_root_points_at_root():
    root = registry.repo_root()
    if not os.path.isabs(root):
        FAILURES.append(f"registry.repo_root() is not absolute: {root!r}")
        return
    missing = [m for m in MARKERS if not os.path.exists(os.path.join(root, m))]
    if missing:
        FAILURES.append(f"registry.repo_root() {root!r} is missing {missing} "
                        "-- wrong dirname depth after a move?")


TESTS = [test_repo_root_points_at_root]


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
