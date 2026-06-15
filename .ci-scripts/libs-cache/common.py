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
import subprocess
import sys

ENDC = '\033[0m'
ERROR = '\033[31m'
INFO = '\033[34m'


def die(message):
    print(ERROR + message + ENDC, file=sys.stderr)
    sys.exit(1)


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


def cache_args(args):
    """The selector (+ tag) passed through to the cache primitive scripts."""
    sel = ['--image', args.image] if args.image else ['--platform', args.platform]
    return sel + ['--tag', args.tag]
