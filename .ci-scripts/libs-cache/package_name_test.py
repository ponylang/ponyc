#!/usr/bin/env python3
"""Self-contained tests for the cache package-name assembly.

Run: python3 .ci-scripts/libs-cache/package_name_test.py
Exits 0 if every check passes, 1 otherwise.

`cache_package` in oci_libs_cache.py / branch_libs_cache.py is the one place the
namespace, the platform label, and the (normalized) arch are joined into the GHCR
package name. A pull and a push must agree on that name byte-for-byte or the cache
silently misses, and the branch and main names must match (apart from the namespace
prefix) or the warmer can't find a promotable artifact. This pins the assembly:
namespace placement and the cache_arch.canonical arch normalization
(amd64->x86_64, aarch64->arm64, uppercase Windows), and the hard error on an
unrecognized arch. platform.machine() is monkeypatched so the result doesn't depend
on the host this runs on.
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
    # The branch name is the main name under a different namespace prefix (no PR
    # component): the warmer relies on this to construct a promotable name.
    cases = [
        ('AMD64', 'windows-2025-vs2026',
         'ponyc-branch-libs-cache/windows-2025-vs2026-x86_64'),
        ('arm64', 'arm64-macos-26',
         'ponyc-branch-libs-cache/arm64-macos-26-arm64'),
    ]
    for machine, label, want in cases:
        got = with_machine(machine, lambda: branch.cache_package(label))
        check(f"branch.cache_package({label!r}) @ {machine}", got, want)


def test_branch_matches_main_modulo_namespace():
    # The promote step constructs the branch name from the main name by swapping
    # the namespace prefix. Pin that they differ ONLY by that prefix (slicing, not
    # str.removeprefix, to stay 3.8-safe like the scripts).
    for machine, label in [('x86_64', 'freebsd-14.3'),
                           ('aarch64', 'alpine3.23'),
                           ('AMD64', 'windows-2025-vs2026')]:
        main = with_machine(machine, lambda: oci.cache_package(label))
        br = with_machine(machine, lambda: branch.cache_package(label))
        main_suffix = main[len(oci.NAMESPACE) + 1:]
        branch_suffix = br[len(branch.NAMESPACE) + 1:]
        check(f"suffix match @ {label}/{machine}", main_suffix, branch_suffix)


def test_unknown_arch_dies():
    # An unrecognized arch must abort (cache_arch.canonical raises -> die ->
    # SystemExit), never mint a non-canonical package name.
    for entry in (lambda: oci.cache_package('x'),
                  lambda: branch.cache_package('x')):
        try:
            result = with_machine('riscv64', entry)
            FAILURES.append(f"unknown arch returned {result!r}, expected exit")
        except SystemExit as e:
            if e.code != 1:
                FAILURES.append(f"unknown arch exited {e.code}, want 1")


TESTS = [test_main_cache_names, test_branch_cache_names,
         test_branch_matches_main_modulo_namespace, test_unknown_arch_dies]


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
