#!/usr/bin/env python3
"""Self-contained tests for resolve_libs_cache.py (no pytest; python3-runnable).

Run: python3 .ci-scripts/libs-cache/resolve_libs_cache_test.py
Exits 0 if every check passes, 1 otherwise.

resolve_libs_cache.py is the consumer/warmer orchestration every non-BSD libs
build job calls. Its `main()` shells the oci_libs_cache.py primitives and the
build command out through `run()`, so monkeypatching `run` lets us assert the
exact primitive sequence and exit code for every mode/path without any
subprocess or network. The load-bearing invariant -- consumer mode never pushes
the main cache (the warmer is its only writer) -- is pinned here.
"""
import sys

import resolve_libs_cache as r

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def run_main(argv, results):
    """Drive r.main with `run` mocked. `results` maps an oci verb
    ('exists'/'pull'/'push') or 'BUILD' to the exit code that call returns.
    Returns (recorded_calls, exit_code)."""
    calls = []

    def fake_run(cmd):
        # An oci primitive call is [sys.executable, MAIN_CACHE, <verb>, ...].
        if len(cmd) >= 3 and cmd[1] == r.MAIN_CACHE:
            calls.append(cmd[2])
            return results.get(cmd[2], 0)
        calls.append('BUILD')
        return results.get('BUILD', 0)

    saved = r.run
    r.run = fake_run
    try:
        r.main(['resolve_libs_cache.py'] + argv)
        code = 0
    except SystemExit as e:
        code = e.code if e.code is not None else 0
    finally:
        r.run = saved
    return calls, code


CONSUMER = ['--platform', 'p', '--tag', 't', '--', 'make', 'libs']
WARM = ['--warm', '--platform', 'p', '--tag', 't', '--', 'make', 'libs']


def test_consumer_hit():
    # pull succeeds -> libs restored, no build, no push.
    calls, code = run_main(CONSUMER, {'pull': 0})
    check('consumer hit calls', calls, ['pull'])
    check('consumer hit code', code, 0)


def test_consumer_miss_builds_never_pushes():
    # pull misses -> build runs. CONSUMER MODE MUST NOT PUSH.
    calls, code = run_main(CONSUMER, {'pull': 1, 'BUILD': 0})
    check('consumer miss calls', calls, ['pull', 'BUILD'])
    check('consumer miss code', code, 0)
    check('consumer never pushes', 'push' in calls, False)


def test_consumer_build_failure_propagates():
    # build's exit code propagates; still no push.
    calls, code = run_main(CONSUMER, {'pull': 1, 'BUILD': 7})
    check('consumer build-fail calls', calls, ['pull', 'BUILD'])
    check('consumer build-fail code', code, 7)


def test_warm_hit():
    # exists succeeds -> done. No download, no build, no push.
    calls, code = run_main(WARM, {'exists': 0})
    check('warm hit calls', calls, ['exists'])
    check('warm hit code', code, 0)


def test_warm_miss_builds_then_pushes():
    # exists misses -> build -> push, in that order.
    calls, code = run_main(WARM, {'exists': 1, 'BUILD': 0, 'push': 0})
    check('warm miss calls', calls, ['exists', 'BUILD', 'push'])
    check('warm miss code', code, 0)


def test_warm_build_failure_no_push():
    # build fails on a warm miss -> propagate, never push.
    calls, code = run_main(WARM, {'exists': 1, 'BUILD': 5})
    check('warm build-fail calls', calls, ['exists', 'BUILD'])
    check('warm build-fail code', code, 5)
    check('warm build-fail no push', 'push' in calls, False)


def test_warm_push_failure_hard_fails():
    # build succeeds but push fails -> die (exit 1).
    calls, code = run_main(WARM, {'exists': 1, 'BUILD': 0, 'push': 1})
    check('warm push-fail calls', calls, ['exists', 'BUILD', 'push'])
    check('warm push-fail code', code, 1)


def test_missing_double_dash():
    # no `--` separator -> die before doing anything.
    calls, code = run_main(['--platform', 'p', '--tag', 't', 'make', 'libs'],
                           {})
    check('missing -- calls', calls, [])
    check('missing -- code', code, 1)


def test_empty_build_command():
    # `--` present but nothing after it -> die.
    calls, code = run_main(['--platform', 'p', '--tag', 't', '--'], {})
    check('empty build calls', calls, [])
    check('empty build code', code, 1)


TESTS = [test_consumer_hit, test_consumer_miss_builds_never_pushes,
         test_consumer_build_failure_propagates, test_warm_hit,
         test_warm_miss_builds_then_pushes, test_warm_build_failure_no_push,
         test_warm_push_failure_hard_fails, test_missing_double_dash,
         test_empty_build_command]


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
