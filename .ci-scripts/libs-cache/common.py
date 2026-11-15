#!/usr/bin/env python3
"""Shared helpers for the libs-cache scripts.

The `.ci-scripts/libs-cache/` directory is a set of entry-point scripts (the
runnable `*_libs_cache.py` / `resolve_libs_cache.py`) over a small support
library. This module is the base of that library: the colored `die`/`info`
printers, the `require_env` reader, and the orchestrator helpers (`run`,
`split_build_command`, `cache_args`) that `resolve_libs_cache.py` and
`pr_libs_cache.py` share. The registry-v2 client lives in `registry.py`, the
GitHub-packages REST client in `ghpackages.py`, and arch normalization in
`cache_arch.py` -- all import from here.

Stdlib only so no pip install is required on any CI runner.
"""

import os
import platform
import subprocess
import sys

ENDC = '\033[0m'
ERROR = '\033[31m'
INFO = '\033[34m'

# Exit codes for the cache primitives (exists, pull, push). Callers that shell
# out to these scripts read 0 as present, EXIT_MISS as a definite cache miss
# (the artifact is not present), and any other code as an error. EXIT_MISS must
# be a code the interpreter and the shell never produce on their own: Python
# exits 1 on an uncaught exception and 2 on a usage error or a script it can't
# open, and a failing shell command can exit 1, so a crash on either would
# otherwise read as a miss. `die` exits EXIT_ERROR for the errors the scripts
# detect themselves.
EXIT_MISS = 4
EXIT_ERROR = 3

# Paths to the cache entry scripts, shared so the orchestrators
# (resolve_libs_cache.py, pr_libs_cache.py) agree on one spelling. They are
# strings, not imports, so there is no import cycle; each orchestrator still calls
# its own `run` on them (which keeps the test monkeypatch of `<module>.run` able to
# intercept the subprocess).
SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
MAIN_CACHE = os.path.join(SCRIPTS_DIR, 'oci_libs_cache.py')
BRANCH_CACHE = os.path.join(SCRIPTS_DIR, 'branch_libs_cache.py')
PROMOTE = os.path.join(SCRIPTS_DIR, 'promote_libs_cache.py')


def die(message):
    print(ERROR + message + ENDC, file=sys.stderr)
    sys.exit(EXIT_ERROR)


def info(message):
    print(INFO + message + ENDC)


def require_env(name):
    value = os.environ.get(name, '')
    if not value:
        die(f"{name} needs to be set in env. Exiting.")
    return value


def run(cmd):
    """Run a subprocess, streaming its output; return the exit code."""
    return subprocess.run(cmd).returncode


def split_build_command(argv):
    """Split argv at the first `--`: (our args, the build command after it)."""
    if '--' not in argv:
        die("missing '--' followed by the build command.")
    cut = argv.index('--')
    return argv[:cut], argv[cut + 1:]


def _msvc_tools_version():
    """Return the MSVC toolchain version on Windows, or None elsewhere.

    The version comes from Microsoft.VCToolsVersion.default.txt in the VS
    installation found by vswhere. When the file or vswhere is absent (not
    Windows, no VS, a non-MSVC toolchain), returns None.
    """
    if platform.system() != 'Windows':
        return None
    for vswhere_dir in (r'C:\Program Files (x86)\Microsoft Visual Studio\Installer',
                        r'C:\Program Files\Microsoft Visual Studio\Installer'):
        vswhere = os.path.join(vswhere_dir, 'vswhere.exe')
        if os.path.isfile(vswhere):
            break
    else:
        return None
    try:
        result = subprocess.run(
            [vswhere, '-latest', '-property', 'installationPath'],
            capture_output=True, text=True, timeout=10)
        vs_path = result.stdout.strip()
        if result.returncode != 0 or not vs_path:
            return None
    except Exception:
        return None
    version_file = os.path.join(
        vs_path, 'VC', 'Auxiliary', 'Build',
        'Microsoft.VCToolsVersion.default.txt')
    try:
        with open(version_file) as f:
            return f.read().strip()
    except Exception:
        return None


def cache_args(args):
    """Build the selector and tag arguments for a cache primitive script.

    On Windows, the MSVC toolchain version is appended to the tag so that LLVM
    libraries compiled under one toolchain version are never linked against a
    different version's STL. The MSVC STL ships vectorized algorithm intrinsics
    (e.g. __std_replace_1) that change between toolchain updates; a cached LLVM
    built with a newer STL fails to link on a runner whose STL is older.
    """
    sel = ['--image', args.image] if args.image else ['--platform', args.platform]
    tag = args.tag
    msvc_ver = _msvc_tools_version()
    if msvc_ver:
        tag = f'{tag}-msvc{msvc_ver}'
    return sel + ['--tag', tag]
