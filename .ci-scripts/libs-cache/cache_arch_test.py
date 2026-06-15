#!/usr/bin/env python3
"""Self-contained tests for cache_arch.py (no pytest; python3-runnable).

Run: python3 .ci-scripts/libs-cache/cache_arch_test.py
Exits 0 if every check passes, 1 otherwise.

One shared module, so this covers both oci_libs_cache.py and branch_libs_cache.py
(they both call cache_arch.canonical).
"""
import sys

import cache_arch as c

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


# (input from platform.machine(), canonical spelling). Both ISA families, both
# spellings each, including the uppercase Windows reports (AMD64 / ARM64).
CANONICAL_TABLE = [
    ('x86_64', 'x86_64'),    # Linux, macOS-intel, DragonFly, cross images
    ('amd64', 'x86_64'),     # FreeBSD, OpenBSD
    ('AMD64', 'x86_64'),     # Windows x64
    ('X86_64', 'x86_64'),    # defensive: any uppercase variant
    ('aarch64', 'arm64'),    # Linux arm
    ('arm64', 'arm64'),      # macOS arm
    ('ARM64', 'arm64'),      # Windows arm
]


def test_canonical_mapping():
    for machine, want in CANONICAL_TABLE:
        check(f"canonical({machine!r})", c.canonical(machine), want)


def test_unknown_raises():
    # An unrecognized arch must RAISE, not silently pass the raw value through --
    # that strictness is the whole point (a new platform fails loudly).
    for machine in ('riscv64', 'ppc64le', 's390x', 'sparc64', ''):
        try:
            result = c.canonical(machine)
            FAILURES.append(f"canonical({machine!r}) returned {result!r}, "
                            "expected ValueError")
        except ValueError as e:
            # The message must name the offending arch so the failure is actionable.
            if machine and machine not in str(e):
                FAILURES.append(f"canonical({machine!r}) ValueError text "
                                f"{str(e)!r} omits the arch")


def test_aliases_are_self_consistent():
    # Every alias target must itself be a canonical key mapping to itself, so the
    # map can never point at a spelling it would then reject.
    for source, target in c.ARCH_ALIASES.items():
        check(f"target {target!r} is canonical", c.ARCH_ALIASES.get(target),
              target)


TESTS = [test_canonical_mapping, test_unknown_raises,
         test_aliases_are_self_consistent]


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
