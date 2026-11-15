#!/usr/bin/env python3
"""Self-contained tests for common.py (no pytest; python3-runnable).

Run: python3 .ci-scripts/libs-cache/common_test.py
Exits 0 if every check passes, 1 otherwise.
"""
import argparse
import os
import sys
import tempfile

import common

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def test_cache_args_non_windows():
    """On non-Windows, cache_args passes the tag through unchanged."""
    saved = common.platform.system
    common.platform.system = lambda: 'Linux'
    try:
        args = argparse.Namespace(image=None, platform='ubuntu-24.04', tag='abc')
        result = common.cache_args(args)
        check("non-windows tag unchanged",
              result, ['--platform', 'ubuntu-24.04', '--tag', 'abc'])
    finally:
        common.platform.system = saved


def test_cache_args_image_selector():
    """--image is forwarded when present."""
    saved = common.platform.system
    common.platform.system = lambda: 'Linux'
    try:
        args = argparse.Namespace(image='ghcr.io/foo:bar', platform=None,
                                  tag='def')
        result = common.cache_args(args)
        check("image selector",
              result, ['--image', 'ghcr.io/foo:bar', '--tag', 'def'])
    finally:
        common.platform.system = saved


def test_msvc_tools_version_not_windows():
    """Returns None when not on Windows."""
    saved = common.platform.system
    common.platform.system = lambda: 'Linux'
    try:
        check("not windows", common._msvc_tools_version(), None)
    finally:
        common.platform.system = saved


def test_msvc_tools_version_no_vswhere():
    """Returns None when vswhere isn't found."""
    saved_system = common.platform.system
    saved_isfile = common.os.path.isfile
    common.platform.system = lambda: 'Windows'
    common.os.path.isfile = lambda p: False
    try:
        check("no vswhere", common._msvc_tools_version(), None)
    finally:
        common.platform.system = saved_system
        common.os.path.isfile = saved_isfile


def test_msvc_tools_version_vswhere_fails():
    """Returns None when vswhere exits non-zero."""
    saved_system = common.platform.system
    saved_isfile = common.os.path.isfile
    saved_run = common.subprocess.run

    common.platform.system = lambda: 'Windows'
    common.os.path.isfile = lambda p: 'vswhere' in p

    class FakeResult:
        returncode = 1
        stdout = ''
    common.subprocess.run = lambda *a, **kw: FakeResult()
    try:
        check("vswhere fails", common._msvc_tools_version(), None)
    finally:
        common.platform.system = saved_system
        common.os.path.isfile = saved_isfile
        common.subprocess.run = saved_run


def test_msvc_tools_version_version_file_missing():
    """Returns None when the version file doesn't exist."""
    saved_system = common.platform.system
    saved_isfile = common.os.path.isfile
    saved_run = common.subprocess.run

    common.platform.system = lambda: 'Windows'
    common.os.path.isfile = lambda p: 'vswhere' in p

    class FakeResult:
        returncode = 0
        stdout = r'C:\NoSuchVS'
    common.subprocess.run = lambda *a, **kw: FakeResult()
    try:
        result = common._msvc_tools_version()
        check("version file missing", result, None)
    finally:
        common.platform.system = saved_system
        common.os.path.isfile = saved_isfile
        common.subprocess.run = saved_run


def test_cache_args_windows_enriches_tag():
    """On Windows with a detectable MSVC version, the tag is enriched."""
    saved_fn = common._msvc_tools_version
    common._msvc_tools_version = lambda: '14.50.35717'
    try:
        args = argparse.Namespace(image=None, platform='windows-11-arm',
                                  tag='abc123')
        result = common.cache_args(args)
        check("windows enriched tag",
              result,
              ['--platform', 'windows-11-arm', '--tag',
               'abc123-msvc14.50.35717'])
    finally:
        common._msvc_tools_version = saved_fn


def test_cache_args_windows_no_msvc_passes_through():
    """On Windows without detectable MSVC, the tag is unchanged."""
    saved_fn = common._msvc_tools_version
    common._msvc_tools_version = lambda: None
    try:
        args = argparse.Namespace(image=None, platform='windows-2025-vs2026',
                                  tag='xyz')
        result = common.cache_args(args)
        check("windows no msvc tag unchanged",
              result,
              ['--platform', 'windows-2025-vs2026', '--tag', 'xyz'])
    finally:
        common._msvc_tools_version = saved_fn


def test_msvc_tools_version_vswhere_empty_stdout():
    """Returns None when vswhere exits 0 but reports no installation."""
    saved_system = common.platform.system
    saved_isfile = common.os.path.isfile
    saved_run = common.subprocess.run

    common.platform.system = lambda: 'Windows'
    common.os.path.isfile = lambda p: 'vswhere' in p

    class FakeResult:
        returncode = 0
        stdout = ''
    common.subprocess.run = lambda *a, **kw: FakeResult()
    try:
        check("vswhere empty stdout", common._msvc_tools_version(), None)
    finally:
        common.platform.system = saved_system
        common.os.path.isfile = saved_isfile
        common.subprocess.run = saved_run


def test_msvc_tools_version_subprocess_raises():
    """Returns None when subprocess.run raises (e.g. timeout or OSError)."""
    saved_system = common.platform.system
    saved_isfile = common.os.path.isfile
    saved_run = common.subprocess.run

    common.platform.system = lambda: 'Windows'
    common.os.path.isfile = lambda p: 'vswhere' in p

    def raise_oserror(*a, **kw):
        raise OSError("fake")
    common.subprocess.run = raise_oserror
    try:
        check("subprocess raises", common._msvc_tools_version(), None)
    finally:
        common.platform.system = saved_system
        common.os.path.isfile = saved_isfile
        common.subprocess.run = saved_run


def test_msvc_tools_version_happy_path():
    """Reads the version from a temporary file tree."""
    saved_system = common.platform.system
    saved_isfile = common.os.path.isfile
    saved_run = common.subprocess.run

    common.platform.system = lambda: 'Windows'
    common.os.path.isfile = lambda p: 'vswhere' in p

    with tempfile.TemporaryDirectory() as tmpdir:
        vc_dir = os.path.join(tmpdir, 'VC', 'Auxiliary', 'Build')
        os.makedirs(vc_dir)
        version_file = os.path.join(
            vc_dir, 'Microsoft.VCToolsVersion.default.txt')
        with open(version_file, 'w') as f:
            f.write('14.42.12345\n')

        class FakeResult:
            returncode = 0
            stdout = tmpdir
        common.subprocess.run = lambda *a, **kw: FakeResult()
        try:
            check("happy path version",
                  common._msvc_tools_version(), '14.42.12345')
        finally:
            common.platform.system = saved_system
            common.os.path.isfile = saved_isfile
            common.subprocess.run = saved_run


def test_split_build_command():
    """split_build_command splits at -- and rejects missing --."""
    ours, build = common.split_build_command(
        ['--flag', 'val', '--', 'cmake', '-P', 'x'])
    check("split ours", ours, ['--flag', 'val'])
    check("split build", build, ['cmake', '-P', 'x'])

    saved_stderr = sys.stderr
    sys.stderr = open(os.devnull, 'w')
    try:
        common.split_build_command(['--flag', 'val'])
        FAILURES.append("split_build_command without -- should have called die")
    except SystemExit:
        pass
    finally:
        sys.stderr.close()
        sys.stderr = saved_stderr


TESTS = [
    test_cache_args_non_windows,
    test_cache_args_image_selector,
    test_msvc_tools_version_not_windows,
    test_msvc_tools_version_no_vswhere,
    test_msvc_tools_version_vswhere_fails,
    test_msvc_tools_version_vswhere_empty_stdout,
    test_msvc_tools_version_subprocess_raises,
    test_msvc_tools_version_version_file_missing,
    test_msvc_tools_version_happy_path,
    test_cache_args_windows_enriches_tag,
    test_cache_args_windows_no_msvc_passes_through,
    test_split_build_command,
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
