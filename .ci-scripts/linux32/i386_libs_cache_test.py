#!/usr/bin/env python3
"""Self-contained tests for i386-libs-cache.bash's host-side exit codes.

Callers read exit 0 as present, EXIT_MISS as a definite cache miss, and any
other code as an error. The script's own failures exit 3 (EXIT_ERROR), and the
cache scripts' exit codes must pass through unchanged. Each case runs a copy of
the script in a temporary directory with a stub `python3` first on PATH.

Run: python3 .ci-scripts/linux32/i386_libs_cache_test.py
Exits 0 if every check passes, 1 otherwise.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'libs-cache'))
from common import EXIT_ERROR, EXIT_MISS  # noqa: E402
SCRIPT = os.path.join(HERE, 'i386-libs-cache.bash')
ENV_FILE = os.path.join(HERE, 'i386-alpine.env')

FAILURES = []

# The exit code of a Python script that dies on an uncaught exception.
PYTHON_CRASH = 1

STUB = """#!/bin/sh
printf '%s\\n' "$*" >> "$STUB_LOG"
exit "${STUB_RC:-0}"
"""


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def run(args, env_file='copy', libs_tag='t', stub_rc=0):
    """Run a copy of the script; return (exit code, stub invocations)."""
    with tempfile.TemporaryDirectory() as tmp:
        shutil.copy(SCRIPT, tmp)
        if env_file == 'copy':
            shutil.copy(ENV_FILE, tmp)
        elif env_file is not None:
            with open(os.path.join(tmp, 'i386-alpine.env'), 'w') as f:
                f.write(env_file)
        bindir = os.path.join(tmp, 'bin')
        os.mkdir(bindir)
        stub = os.path.join(bindir, 'python3')
        with open(stub, 'w') as f:
            f.write(STUB)
        os.chmod(stub, 0o755)
        log = os.path.join(tmp, 'stub.log')
        env = dict(os.environ)
        env['PATH'] = bindir + os.pathsep + env['PATH']
        env['STUB_LOG'] = log
        env['STUB_RC'] = str(stub_rc)
        env.pop('LIBS_TAG', None)
        if libs_tag is not None:
            env['LIBS_TAG'] = libs_tag
        proc = subprocess.run(
            ['bash', os.path.join(tmp, 'i386-libs-cache.bash'), *args],
            cwd=tmp, env=env, capture_output=True)
        calls = []
        if os.path.exists(log):
            with open(log) as f:
                calls = f.read().splitlines()
        return proc.returncode, calls


HOST_OPS = ('exists-main', 'exists-branch', 'promote')


def test_own_errors_exit_3():
    check("no operation", run([])[0], EXIT_ERROR)
    check("unknown operation", run(['bogus'])[0], EXIT_ERROR)
    for op in HOST_OPS:
        check(f"{op}, no env file", run([op], env_file=None)[0], EXIT_ERROR)
        check(f"{op}, env file without ALPINE_BRANCH",
              run([op], env_file='ALPINE_RELEASE=3.24.4\n')[0], EXIT_ERROR)
        check(f"{op}, LIBS_TAG unset", run([op], libs_tag=None)[0], EXIT_ERROR)
        check(f"{op}, LIBS_TAG empty", run([op], libs_tag='')[0], EXIT_ERROR)


def test_own_errors_skip_the_cache_script():
    for op in HOST_OPS:
        check(f"{op}, LIBS_TAG unset, stub calls",
              run([op], libs_tag=None)[1], [])


def test_cache_exit_codes_pass_through():
    for op in HOST_OPS:
        for rc in (0, EXIT_MISS, EXIT_ERROR, PYTHON_CRASH):
            check(f"{op}, cache script exits {rc}",
                  run([op], stub_rc=rc)[0], rc)


def test_host_ops_name_the_i386_package():
    for op in HOST_OPS:
        calls = run([op])[1]
        check(f"{op}, one cache call", len(calls), 1)
        if calls:
            words = calls[0].split()
            check(f"{op}, --arch", '--arch i686' in calls[0], True)
            check(f"{op}, --tag", words[-2:], ['--tag', 't'])


TESTS = [
    test_own_errors_exit_3,
    test_own_errors_skip_the_cache_script,
    test_cache_exit_codes_pass_through,
    test_host_ops_name_the_i386_package,
]


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
