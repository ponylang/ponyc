#!/usr/bin/env python3
"""Self-contained tests for pr_libs_cache.py (no pytest; python3-runnable).

Run: python3 .ci-scripts/libs-cache/pr_libs_cache_test.py
Exits 0 if every check passes, 1 otherwise.

pr_libs_cache.py is PR CI's libs orchestration. Its `main()` shells the
oci_libs_cache.py / branch_libs_cache.py primitives and the build command out
through `run()`, so monkeypatching `run` pins the exact primitive sequence without
any subprocess or network. Load-bearing invariants:
  - the branch primitive is invoked WITHOUT a `--pr` (the cache is tag-addressable
    now; a stray `--pr` would make branch_libs_cache.py argparse-reject and break
    every non-fork PR job -- this is the regression guard for the rename);
  - `--branch-cache` (non-fork) does main -> branch -> build -> push-branch; a fork
    (no `--branch-cache`) skips all branch ops and just pulls main or builds;
  - default (non-`--ensure`) branch push is best-effort (a failure warns, exit 0);
    `--ensure` HARD-fails on a branch push failure; `--ensure` requires
    `--branch-cache`.
"""
import contextlib
import io
import sys

from common import EXIT_ERROR, EXIT_MISS
import pr_libs_cache as pr

FAILURES = []

# The exit code of a Python script that dies on an uncaught exception.
PYTHON_CRASH = 1


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def run_main(argv, results, branch_argvs=None):
    """Drive pr.main with `run` mocked. Records labels 'main:<verb>' /
    'branch:<verb>' / 'BUILD'. If branch_argvs is a list, every branch primitive's
    full argv is appended to it (so a test can assert no '--pr')."""
    calls = []

    def fake_run(cmd):
        if len(cmd) >= 3 and cmd[1] == pr.MAIN_CACHE:
            label = 'main:' + cmd[2]
        elif len(cmd) >= 3 and cmd[1] == pr.BRANCH_CACHE:
            label = 'branch:' + cmd[2]
            if branch_argvs is not None:
                branch_argvs.append(cmd)
        else:
            label = 'BUILD'
        calls.append(label)
        return results.get(label, 0)

    saved = pr.run
    pr.run = fake_run
    try:
        pr.main(['pr_libs_cache.py'] + argv)
        code = 0
    except SystemExit as e:
        code = e.code if e.code is not None else 0
    finally:
        pr.run = saved
    return calls, code


# Non-fork passes --branch-cache; a fork omits it.
NONFORK = ['--branch-cache', '--platform', 'p', '--tag', 't', '--', 'cmake']
FORK = ['--platform', 'p', '--tag', 't', '--', 'cmake']
ENSURE = ['--ensure', '--branch-cache', '--platform', 'p', '--tag', 't', '--',
          'cmake']


def test_nonfork_main_hit():
    calls, code = run_main(NONFORK, {'main:pull': 0})
    check('main-hit calls', calls, ['main:pull'])
    check('main-hit code', code, 0)


def test_nonfork_branch_reuse():
    calls, code = run_main(NONFORK, {'main:pull': EXIT_MISS, 'branch:pull': 0})
    check('branch-reuse calls', calls, ['main:pull', 'branch:pull'])
    check('branch-reuse code', code, 0)
    check('branch-reuse no build', 'BUILD' in calls, False)


def test_nonfork_both_miss_builds_pushes_branch_without_pr():
    branch_argvs = []
    calls, code = run_main(NONFORK, {'main:pull': EXIT_MISS,
                                     'branch:pull': EXIT_MISS, 'BUILD': 0,
                                     'branch:push': 0}, branch_argvs)
    check('both-miss calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('both-miss code', code, 0)
    # The rename regression guard: NO branch invocation may carry --pr.
    for argv in branch_argvs:
        check(f"no --pr in {argv[2:]}", '--pr' in argv, False)
    check('branch invoked twice', len(branch_argvs), 2)


def test_nonfork_branch_push_failure_best_effort():
    # default mode: a branch push failure warns but does NOT fail the job.
    calls, code = run_main(NONFORK, {'main:pull': EXIT_MISS,
                                     'branch:pull': EXIT_MISS, 'BUILD': 0,
                                     'branch:push': 1})
    check('push-fail calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('push-fail code', code, 0)


def test_nonfork_build_failure_propagates_no_push():
    calls, code = run_main(NONFORK, {'main:pull': EXIT_MISS,
                                     'branch:pull': EXIT_MISS, 'BUILD': 7})
    check('build-fail calls', calls, ['main:pull', 'branch:pull', 'BUILD'])
    check('build-fail code', code, 7)
    check('build-fail no push', 'branch:push' in calls, False)


def test_fork_skips_branch_ops():
    # No --branch-cache -> pull main, build, NO branch pull/push.
    calls, code = run_main(FORK, {'main:pull': EXIT_MISS, 'BUILD': 0})
    check('fork calls', calls, ['main:pull', 'BUILD'])
    check('fork code', code, 0)
    check('fork no branch', any('branch' in c for c in calls), False)


def test_fork_main_hit():
    calls, code = run_main(FORK, {'main:pull': 0})
    check('fork main-hit calls', calls, ['main:pull'])
    check('fork main-hit code', code, 0)


def test_ensure_uses_exists_and_pushes():
    calls, code = run_main(ENSURE, {'main:exists': EXIT_MISS,
                                    'branch:exists': EXIT_MISS,
                                    'BUILD': 0, 'branch:push': 0})
    check('ensure calls', calls,
          ['main:exists', 'branch:exists', 'BUILD', 'branch:push'])
    check('ensure code', code, 0)


def test_ensure_branch_push_failure_hard_fails():
    calls, code = run_main(ENSURE, {'main:exists': EXIT_MISS,
                                    'branch:exists': EXIT_MISS,
                                    'BUILD': 0, 'branch:push': 1})
    check('ensure push-fail calls', calls,
          ['main:exists', 'branch:exists', 'BUILD', 'branch:push'])
    check('ensure push-fail code', code, EXIT_ERROR)


def test_ensure_requires_branch_cache():
    calls, code = run_main(['--ensure', '--platform', 'p', '--tag', 't', '--',
                            'cmake'], {})
    check('ensure-no-branch calls', calls, [])
    check('ensure-no-branch code', code, EXIT_ERROR)


def test_nonfork_main_error_branch_hit():
    # main errors, branch hits -> restored from branch, no build.
    calls, code = run_main(NONFORK, {'main:pull': EXIT_ERROR, 'branch:pull': 0})
    check('main-error branch-hit calls', calls, ['main:pull', 'branch:pull'])
    check('main-error branch-hit code', code, 0)
    check('main-error branch-hit no build', 'BUILD' in calls, False)


def test_nonfork_main_error_still_builds():
    # An error on main pull still falls through to build.
    calls, code = run_main(NONFORK, {'main:pull': EXIT_ERROR,
                                     'branch:pull': EXIT_MISS, 'BUILD': 0,
                                     'branch:push': 0})
    check('main-error calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('main-error code', code, 0)


def test_nonfork_branch_error_still_builds():
    # main misses, branch errors -> still builds.
    calls, code = run_main(NONFORK, {'main:pull': EXIT_MISS,
                                     'branch:pull': EXIT_ERROR, 'BUILD': 0,
                                     'branch:push': 0})
    check('branch-error calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('branch-error code', code, 0)


def test_ensure_main_error_still_builds():
    # An error on main exists still falls through to build.
    calls, code = run_main(ENSURE, {'main:exists': EXIT_ERROR,
                                    'branch:exists': EXIT_MISS, 'BUILD': 0,
                                    'branch:push': 0})
    check('ensure main-error calls', calls,
          ['main:exists', 'branch:exists', 'BUILD', 'branch:push'])
    check('ensure main-error code', code, 0)


def test_nonfork_crash_reported_as_error():
    # A primitive that crashes exits with Python's uncaught-exception code. That
    # is an error, so it's logged as a failed pull rather than a miss.
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        calls, code = run_main(NONFORK, {'main:pull': PYTHON_CRASH,
                                         'branch:pull': PYTHON_CRASH,
                                         'BUILD': 0, 'branch:push': 0})
    check('crash calls', calls,
          ['main:pull', 'branch:pull', 'BUILD', 'branch:push'])
    check('crash code', code, 0)
    check('crash warns main', 'main cache pull failed (exit 1)' in
          out.getvalue(), True)
    check('crash warns branch', 'branch cache pull failed (exit 1)' in
          out.getvalue(), True)
    check('crash not logged as a miss', 'Libs cache miss' in out.getvalue(),
          False)


def test_missing_double_dash():
    calls, code = run_main(['--branch-cache', '--platform', 'p', '--tag', 't',
                            'cmake'], {})
    check('missing -- calls', calls, [])
    check('missing -- code', code, EXIT_ERROR)


def test_empty_build_command():
    calls, code = run_main(['--branch-cache', '--platform', 'p', '--tag', 't',
                            '--'], {})
    check('empty build calls', calls, [])
    check('empty build code', code, EXIT_ERROR)


TESTS = [test_nonfork_main_hit, test_nonfork_branch_reuse,
         test_nonfork_both_miss_builds_pushes_branch_without_pr,
         test_nonfork_branch_push_failure_best_effort,
         test_nonfork_build_failure_propagates_no_push,
         test_fork_skips_branch_ops, test_fork_main_hit,
         test_ensure_uses_exists_and_pushes,
         test_ensure_branch_push_failure_hard_fails,
         test_ensure_requires_branch_cache,
         test_nonfork_main_error_branch_hit,
         test_nonfork_main_error_still_builds,
         test_nonfork_branch_error_still_builds,
         test_ensure_main_error_still_builds,
         test_nonfork_crash_reported_as_error,
         test_missing_double_dash,
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
