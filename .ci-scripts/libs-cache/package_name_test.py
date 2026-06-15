#!/usr/bin/env python3
"""Self-contained tests for the cache package-name assembly.

Run: python3 .ci-scripts/libs-cache/package_name_test.py
Exits 0 if every check passes, 1 otherwise.

`cache_package` in oci_libs_cache.py / branch_libs_cache.py is the one place the
namespace, the platform label, the (normalized) arch, and -- for the branch
cache -- the PR number are joined into the GHCR package name. A pull and a push
must agree on that name byte-for-byte or the cache silently misses. This pins the
assembly: namespace placement, the cache_arch.canonical arch normalization
(amd64->x86_64, aarch64->arm64, uppercase Windows), the branch `-pr<N>` suffix,
and the hard error on an unrecognized arch. platform.machine() is monkeypatched
so the result doesn't depend on the host this runs on.
"""
import platform
import sys

import branch_libs_cache as branch
import oci_libs_cache as oci

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def with_machine(machine, fn):
    """Run fn() with platform.machine() forced to `machine`."""
    saved = platform.machine
    platform.machine = lambda: machine
    try:
        return fn()
    finally:
        platform.machine = saved


def test_main_cache_names():
    cases = [
        ('amd64', 'freebsd-14.3', 'ponyc-libs-cache/freebsd-14.3-x86_64'),
        ('x86_64', 'x86-macos-15-intel',
         'ponyc-libs-cache/x86-macos-15-intel-x86_64'),
        ('aarch64', 'alpine3.23', 'ponyc-libs-cache/alpine3.23-arm64'),
        ('ARM64', 'arm64-macos-26', 'ponyc-libs-cache/arm64-macos-26-arm64'),
    ]
    for machine, label, want in cases:
        got = with_machine(machine, lambda: oci.cache_package(label))
        check(f"oci.cache_package({label!r}) @ {machine}", got, want)


def test_branch_cache_names():
    cases = [
        ('AMD64', 'windows-2025-vs2026', 5,
         'ponyc-branch-libs-cache/windows-2025-vs2026-x86_64-pr5'),
        ('arm64', 'arm64-macos-26', 9,
         'ponyc-branch-libs-cache/arm64-macos-26-arm64-pr9'),
    ]
    for machine, label, pr, want in cases:
        got = with_machine(machine, lambda: branch.cache_package(label, pr))
        check(f"branch.cache_package({label!r}, {pr}) @ {machine}", got, want)


def test_unknown_arch_dies():
    # An unrecognized arch must abort (cache_arch.canonical raises -> die ->
    # SystemExit), never mint a non-canonical package name.
    for entry in (lambda: oci.cache_package('x'),
                  lambda: branch.cache_package('x', 1)):
        try:
            result = with_machine('riscv64', entry)
            FAILURES.append(f"unknown arch returned {result!r}, expected exit")
        except SystemExit as e:
            if e.code != 1:
                FAILURES.append(f"unknown arch exited {e.code}, want 1")


TESTS = [test_main_cache_names, test_branch_cache_names, test_unknown_arch_dies]


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
