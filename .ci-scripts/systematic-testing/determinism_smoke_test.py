#!/usr/bin/env python3
"""Unit tests for determinism_smoke.py.

Self-contained (no pytest); run as
`python3 .ci-scripts/systematic-testing/determinism_smoke_test.py`. Exits 0 on
pass, 1 on failure, and prints a one-line summary. Covers the pure helpers
(parse_order_sig / is_reproducible / explores), including the counterfactual
cases that must read False; the build/run orchestration needs a built ponyc and
is exercised end-to-end by the weekly job.
"""

import determinism_smoke as smoke

CASES = [
    # parse_order_sig
    ("parse: well-formed line",
     smoke.parse_order_sig(b"RECEIVED=512 ORDER_SIG=15143612403306820233\n"),
     "15143612403306820233"),
    ("parse: amid other output",
     smoke.parse_order_sig(b"junk\nRECEIVED=1 ORDER_SIG=42\nmore\n"), "42"),
    ("parse: first match wins",
     smoke.parse_order_sig(b"ORDER_SIG=111\nORDER_SIG=222\n"), "111"),
    ("parse: absent -> None", smoke.parse_order_sig(b"no signature here"), None),
    ("parse: empty -> None", smoke.parse_order_sig(b""), None),

    # is_reproducible
    ("reproducible: all identical", smoke.is_reproducible(["a", "a", "a"]), True),
    ("reproducible: single run", smoke.is_reproducible(["a"]), True),
    # counterfactual: a differing run must NOT read as reproducible
    ("reproducible: a differing run -> False",
     smoke.is_reproducible(["a", "b", "a"]), False),
    ("reproducible: no runs -> False", smoke.is_reproducible([]), False),

    # explores
    ("explores: distinct signatures", smoke.explores(["a", "b", "c"]), True),
    ("explores: two distinct", smoke.explores(["a", "b"]), True),
    # counterfactual: a collapsed search (all same) must NOT read as exploring
    ("explores: all identical -> False", smoke.explores(["a", "a", "a"]), False),
    ("explores: single seed -> False", smoke.explores(["a"]), False),
    ("explores: no seeds -> False", smoke.explores([]), False),
]


def run():
    failures = ["%s: expected %r, got %r" % (desc, expected, actual)
                for desc, actual, expected in CASES if actual != expected]
    for f in failures:
        print("FAIL: " + f)
    print("%d/%d checks passed" % (len(CASES) - len(failures), len(CASES)))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(run())
