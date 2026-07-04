#!/usr/bin/env python3
"""Self-contained tests for resolve_libs_cache.py (no pytest; python3-runnable).

Run: python3 .ci-scripts/libs-cache/resolve_libs_cache_test.py
Exits 0 if every check passes, 1 otherwise.

resolve_libs_cache.py is the consumer/warmer/require-cache-hit orchestration the
non-PR libs build jobs call. Its `main()` shells the oci_libs_cache.py /
branch_libs_cache.py / promote_libs_cache.py primitives and the build command out
through `run()`, so monkeypatching `run` lets us assert the exact primitive
sequence and exit code for every mode/path without any subprocess or network. The
load-bearing invariants pinned here:
  - consumer mode never pushes the MAIN cache (the warmer is its only writer);
  - the warmer promotes a branch artifact on a main-cache miss before
    cold-building, and a failed promote falls through to build+push rather than
    failing the job;
  - `--branch-cache` consumer mode pulls the branch cache for reuse and pushes it
    best-effort (a push failure does NOT fail the job), and a MAIN hit short-circuits
    before any branch op (so the flag self-gates on a warm main cache);
  - require-cache-hit mode never builds: a miss fails loudly by default, pulls the
    branch cache first when --branch-cache is set (manual branch runs), or skips
    quietly (writes the .libs-cache-miss marker, exit 0) under --skip-on-miss (the
    scheduled stress loop).
"""
import os
import shutil
import sys
import tempfile

import resolve_libs_cache as r

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def run_main(argv, results):
    """Drive r.main with `run` mocked. `results` maps a recorded label to the exit
    code that call returns. Labels: 'main:<verb>' / 'branch:<verb>' for the cache
    primitives, 'promote' for the promote script, 'BUILD' for the build command.
    Returns (recorded_calls, exit_code)."""
    calls = []

    def fake_run(cmd):
        if len(cmd) >= 3 and cmd[1] == r.MAIN_CACHE:
            label = 'main:' + cmd[2]
        elif len(cmd) >= 3 and cmd[1] == r.BRANCH_CACHE:
            label = 'branch:' + cmd[2]
        elif len(cmd) >= 2 and cmd[1] == r.PROMOTE:
            label = 'promote'
        else:
            label = 'BUILD'
        calls.append(label)
        return results.get(label, 0)

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


def run_main_ws(argv, results):
    """Like run_main, but with GITHUB_WORKSPACE pointed at a fresh temp dir so a
    --skip-on-miss marker write is captured there (never in the repo). Returns
    (calls, code, marker_written). Restores env and removes the temp dir."""
    tmp = tempfile.mkdtemp()
    saved_ws = os.environ.get('GITHUB_WORKSPACE')
    os.environ['GITHUB_WORKSPACE'] = tmp
    try:
        calls, code = run_main(argv, results)
        marker = os.path.exists(os.path.join(tmp, '.libs-cache-miss'))
    finally:
        if saved_ws is None:
            os.environ.pop('GITHUB_WORKSPACE', None)
        else:
            os.environ['GITHUB_WORKSPACE'] = saved_ws
        shutil.rmtree(tmp, ignore_errors=True)
    return calls, code, marker


CONSUMER = ['--platform', 'p', '--tag', 't', '--', 'make', 'libs']
BRANCH = ['--branch-cache', '--platform', 'p', '--tag', 't', '--', 'make', 'libs']
WARM = ['--warm', '--platform', 'p', '--tag', 't', '--', 'make', 'libs']
REQUIRE_HIT = ['--require-cache-hit', '--platform', 'p', '--tag', 't']
REQUIRE_HIT_IMAGE = ['--require-cache-hit', '--image', 'i', '--tag', 't']
REQUIRE_HIT_BRANCH = ['--require-cache-hit', '--branch-cache',
                      '--platform', 'p', '--tag', 't']
REQUIRE_HIT_SKIP = ['--require-cache-hit', '--skip-on-miss',
                    '--platform', 'p', '--tag', 't']
REQUIRE_HIT_BRANCH_SKIP = ['--require-cache-hit', '--branch-cache',
                           '--skip-on-miss', '--platform', 'p', '--tag', 't']


# ----- plain consumer (no branch cache) -----

def test_consumer_hit():
    calls, code = run_main(CONSUMER, {'main:pull': 0})
    check('consumer hit calls', calls, ['main:pull'])
    check('consumer hit code', code, 0)


def test_consumer_miss_builds_never_pushes():
    # pull misses -> build runs. CONSUMER MODE MUST NOT PUSH and MUST NOT touch the
    # branch cache without --branch-cache.
    calls, code = run_main(CONSUMER, {'main:pull': 1, 'BUILD': 0})
    check('consumer miss calls', calls, ['main:pull', 'BUILD'])
    check('consumer miss code', code, 0)
    check('consumer never pushes', any('push' in c for c in calls), False)
    check('consumer no branch', any('branch' in c for c in calls), False)


def test_consumer_build_failure_propagates():
    calls, code = run_main(CONSUMER, {'main:pull': 1, 'BUILD': 7})
    check('consumer build-fail calls', calls, ['main:pull', 'BUILD'])
    check('consumer build-fail code', code, 7)


# ----- consumer with --branch-cache -----

def test_branch_cache_main_hit_short_circuits():
    # A MAIN hit returns before any branch op -> the flag self-gates on a warm
    # main cache.
    calls, code = run_main(BRANCH, {'main:pull': 0})
    check('branch main-hit calls', calls, ['main:pull'])
    check('branch main-hit code', code, 0)


def test_branch_cache_reuse_on_main_miss():
    # main-cache miss -> branch hit -> reuse, no build, no push.
    calls, code = run_main(BRANCH, {'main:pull': 1, 'branch:pull': 0})
    check('branch reuse calls', calls, ['main:pull', 'branch:pull'])
    check('branch reuse code', code, 0)
    check('branch reuse no build', 'BUILD' in calls, False)


def test_branch_cache_both_miss_builds_then_pushes_branch():
    calls, code = run_main(BRANCH, {'main:pull': 1, 'branch:pull': 1, 'BUILD': 0,
                                    'branch:push': 0})
    check('branch both-miss calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('branch both-miss code', code, 0)
    check('branch both-miss no main push', 'main:push' in calls, False)


def test_branch_cache_push_failure_tolerated():
    # branch push fails -> best-effort: warn, do NOT fail the job.
    calls, code = run_main(BRANCH, {'main:pull': 1, 'branch:pull': 1, 'BUILD': 0,
                                    'branch:push': 1})
    check('branch push-fail calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('branch push-fail code', code, 0)


def test_branch_cache_build_failure_propagates_no_push():
    calls, code = run_main(BRANCH, {'main:pull': 1, 'branch:pull': 1, 'BUILD': 5})
    check('branch build-fail calls', calls, ['main:pull', 'branch:pull', 'BUILD'])
    check('branch build-fail code', code, 5)
    check('branch build-fail no push', 'branch:push' in calls, False)


# ----- warmer (--warm) -----

def test_warm_hit():
    calls, code = run_main(WARM, {'main:exists': 0})
    check('warm hit calls', calls, ['main:exists'])
    check('warm hit code', code, 0)


def test_warm_promotes_on_branch_hit():
    # main-cache miss -> branch hit -> promote -> done. No build, no main push.
    calls, code = run_main(WARM, {'main:exists': 1, 'branch:exists': 0,
                                  'promote': 0})
    check('warm promote calls', calls,
          ['main:exists', 'branch:exists', 'promote'])
    check('warm promote code', code, 0)
    check('warm promote no build', 'BUILD' in calls, False)
    check('warm promote no main push', 'main:push' in calls, False)


def test_warm_promote_failure_falls_through_to_build():
    # branch hit but promote fails -> build + push the main cache (best-effort
    # promote, never fatal).
    calls, code = run_main(WARM, {'main:exists': 1, 'branch:exists': 0,
                                  'promote': 1, 'BUILD': 0, 'main:push': 0})
    check('warm promote-fail calls', calls,
          ['main:exists', 'branch:exists', 'promote', 'BUILD', 'main:push'])
    check('warm promote-fail code', code, 0)


def test_warm_branch_miss_builds_then_pushes_main():
    calls, code = run_main(WARM, {'main:exists': 1, 'branch:exists': 1,
                                  'BUILD': 0, 'main:push': 0})
    check('warm branch-miss calls', calls,
          ['main:exists', 'branch:exists', 'BUILD', 'main:push'])
    check('warm branch-miss code', code, 0)


def test_warm_build_failure_no_push():
    calls, code = run_main(WARM, {'main:exists': 1, 'branch:exists': 1, 'BUILD': 5})
    check('warm build-fail calls', calls,
          ['main:exists', 'branch:exists', 'BUILD'])
    check('warm build-fail code', code, 5)
    check('warm build-fail no push', 'main:push' in calls, False)


def test_warm_push_failure_hard_fails():
    calls, code = run_main(WARM, {'main:exists': 1, 'branch:exists': 1, 'BUILD': 0,
                                  'main:push': 1})
    check('warm push-fail calls', calls,
          ['main:exists', 'branch:exists', 'BUILD', 'main:push'])
    check('warm push-fail code', code, 1)


# ----- require-cache-hit -----

def test_require_hit_hit():
    calls, code = run_main(REQUIRE_HIT, {'main:pull': 0})
    check('require-hit hit calls', calls, ['main:pull'])
    check('require-hit hit code', code, 0)


def test_require_hit_miss_dies_never_builds():
    calls, code = run_main(REQUIRE_HIT, {'main:pull': 1})
    check('require-hit miss calls', calls, ['main:pull'])
    check('require-hit miss code', code, 1)
    check('require-hit never builds', 'BUILD' in calls, False)
    check('require-hit never pushes', any('push' in c for c in calls), False)


def test_require_hit_hit_with_image():
    calls, code = run_main(REQUIRE_HIT_IMAGE, {'main:pull': 0})
    check('require-hit image hit calls', calls, ['main:pull'])
    check('require-hit image hit code', code, 0)


def test_require_hit_rejects_build_command():
    calls, code = run_main(REQUIRE_HIT + ['--', 'make', 'libs'], {})
    check('require-hit build-cmd calls', calls, [])
    check('require-hit build-cmd code', code, 1)


# ----- require-cache-hit + --branch-cache (manual branch runs) -----

def test_require_hit_branch_main_hit_short_circuits():
    # A MAIN hit returns before any branch op (same as a PR consumer).
    calls, code = run_main(REQUIRE_HIT_BRANCH, {'main:pull': 0})
    check('require-hit branch main-hit calls', calls, ['main:pull'])
    check('require-hit branch main-hit code', code, 0)


def test_require_hit_branch_reuse_on_main_miss():
    # main-cache miss -> branch hit -> run against the branch-built libs, no fail.
    calls, code = run_main(REQUIRE_HIT_BRANCH, {'main:pull': 1, 'branch:pull': 0})
    check('require-hit branch reuse calls', calls, ['main:pull', 'branch:pull'])
    check('require-hit branch reuse code', code, 0)


def test_require_hit_branch_both_miss_dies_never_builds():
    # neither cache has it for this target -> fail loudly (no --skip-on-miss).
    calls, code = run_main(REQUIRE_HIT_BRANCH, {'main:pull': 1, 'branch:pull': 1})
    check('require-hit branch both-miss calls', calls, ['main:pull', 'branch:pull'])
    check('require-hit branch both-miss code', code, 1)
    check('require-hit branch both-miss never builds', 'BUILD' in calls, False)
    check('require-hit branch both-miss never pushes',
          any('push' in c for c in calls), False)


# ----- require-cache-hit + --skip-on-miss (scheduled stress loop) -----

def test_require_hit_skip_hit_no_marker():
    calls, code, marker = run_main_ws(REQUIRE_HIT_SKIP, {'main:pull': 0})
    check('require-hit skip hit calls', calls, ['main:pull'])
    check('require-hit skip hit code', code, 0)
    check('require-hit skip hit no marker', marker, False)


def test_require_hit_skip_miss_writes_marker_exits_zero():
    # miss -> write the marker and exit 0 (the workflow skips build/run), no build.
    calls, code, marker = run_main_ws(REQUIRE_HIT_SKIP, {'main:pull': 1})
    check('require-hit skip miss calls', calls, ['main:pull'])
    check('require-hit skip miss code', code, 0)
    check('require-hit skip miss wrote marker', marker, True)
    check('require-hit skip miss never builds', 'BUILD' in calls, False)


def test_require_hit_branch_skip_both_miss_skips():
    # --branch-cache + --skip-on-miss: branch is tried before the quiet skip.
    calls, code, marker = run_main_ws(REQUIRE_HIT_BRANCH_SKIP,
                                      {'main:pull': 1, 'branch:pull': 1})
    check('require-hit branch+skip calls', calls, ['main:pull', 'branch:pull'])
    check('require-hit branch+skip code', code, 0)
    check('require-hit branch+skip wrote marker', marker, True)


# ----- flag validation -----

def test_branch_cache_rejected_with_warm():
    calls, code = run_main(['--warm', '--branch-cache', '--platform', 'p',
                            '--tag', 't', '--', 'make', 'libs'], {})
    check('branch+warm calls', calls, [])
    check('branch+warm code', code, 1)


def test_skip_on_miss_rejected_without_require_hit():
    # --skip-on-miss is require-cache-hit only; rejected in consumer/warm mode.
    calls, code = run_main(['--skip-on-miss', '--platform', 'p', '--tag', 't',
                            '--', 'make', 'libs'], {})
    check('skip-on-miss reject calls', calls, [])
    check('skip-on-miss reject code', code, 1)


def test_missing_double_dash():
    calls, code = run_main(['--platform', 'p', '--tag', 't', 'make', 'libs'], {})
    check('missing -- calls', calls, [])
    check('missing -- code', code, 1)


def test_empty_build_command():
    calls, code = run_main(['--platform', 'p', '--tag', 't', '--'], {})
    check('empty build calls', calls, [])
    check('empty build code', code, 1)


TESTS = [test_consumer_hit, test_consumer_miss_builds_never_pushes,
         test_consumer_build_failure_propagates,
         test_branch_cache_main_hit_short_circuits,
         test_branch_cache_reuse_on_main_miss,
         test_branch_cache_both_miss_builds_then_pushes_branch,
         test_branch_cache_push_failure_tolerated,
         test_branch_cache_build_failure_propagates_no_push,
         test_warm_hit, test_warm_promotes_on_branch_hit,
         test_warm_promote_failure_falls_through_to_build,
         test_warm_branch_miss_builds_then_pushes_main,
         test_warm_build_failure_no_push, test_warm_push_failure_hard_fails,
         test_require_hit_hit, test_require_hit_miss_dies_never_builds,
         test_require_hit_hit_with_image, test_require_hit_rejects_build_command,
         test_require_hit_branch_main_hit_short_circuits,
         test_require_hit_branch_reuse_on_main_miss,
         test_require_hit_branch_both_miss_dies_never_builds,
         test_require_hit_skip_hit_no_marker,
         test_require_hit_skip_miss_writes_marker_exits_zero,
         test_require_hit_branch_skip_both_miss_skips,
         test_branch_cache_rejected_with_warm,
         test_skip_on_miss_rejected_without_require_hit,
         test_missing_double_dash, test_empty_build_command]


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
