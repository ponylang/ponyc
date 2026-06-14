#!/usr/bin/env python3
"""Resolve the prebuilt LLVM `build/libs` for a pull-request CI job.

The PR libs-building jobs need the prebuilt LLVM, but -- unlike every other
consumer -- a PR branch that changes an LLVM-determining input misses the main
cache on every push (the main cache only updates on push-to-main). So a non-fork
PR also gets a per-PR scratch cache (`branch_libs_cache.py`): build LLVM once on
the first push that misses, reuse it on later pushes.

This is the orchestration that ties the two caches together for a PR job. It is
CI-only logic -- it keys on a PR number and pushes scratch artifacts -- so it
lives here, in a script the workflow calls, NOT in the Makefile / make.ps1 (which
are the developer-facing build files and know only how to *build* libs). It does
not reimplement any caching itself: it just sequences the existing
`oci_libs_cache.py` and `branch_libs_cache.py` primitives and shells out to the
build command the workflow hands it.

Flow (per platform, per job):
  1. check the main cache             -> hit -> done, no build, no push.
  2. (non-fork) check this PR's branch -> hit -> done, no build, no push.
  3. miss -> run the build command -> (non-fork) push to this PR's branch cache.

Two modes select how "check" and "push failure" behave:

  - consumer mode (default): step 1/2 `pull` (download the blob into the working
    tree on a hit) and the branch push is best-effort -- the libs are already
    built, so a cache-write hiccup degrades to "rebuild next push" rather than
    failing the job.
  - maybe-build mode (`--ensure`, non-fork only): step 1/2 `exists` (no download
    -- the job only needs to know whether to build, not the blob itself, so a hit
    is seconds) and a branch push failure HARD-fails the job, so a registry write
    problem surfaces instead of silently leaving consumers to each cold-build.

`--pr` is empty for forks (and the build command still runs), so a fork behaves
exactly like the plain pull-main-or-build: no branch pull, no push; `--ensure` is
rejected for forks. A build failure does fail the job. A main-cache hit
short-circuits before any push, so a PR job never writes the main cache (the
warmer stays its only writer).

Stdlib only. Usage:
    pr_libs_cache.py [--ensure] (--image <ref> | --platform <label>) \
        --tag <hash> [--pr <N>] -- <build command...>

Everything after `--` is the build command, run as-is on a cache miss (e.g.
`make libs llvm_tools=false build_flags=-j4`, or on Windows
`pwsh -File make.ps1 -Command libs -LlvmTools false`). Auth for the pulls/pushes
uses GITHUB_TOKEN; the branch push needs `packages: write`, which only non-fork
PR runs get.
"""

import argparse
import os
import subprocess
import sys

ENDC = '\033[0m'
ERROR = '\033[31m'
INFO = '\033[34m'

SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
MAIN_CACHE = os.path.join(SCRIPTS_DIR, 'oci_libs_cache.py')
BRANCH_CACHE = os.path.join(SCRIPTS_DIR, 'branch_libs_cache.py')


def die(message):
    print(ERROR + message + ENDC, file=sys.stderr)
    sys.exit(1)


def info(message):
    print(INFO + message + ENDC)


def split_build_command(argv):
    """Split argv at the first `--`: (our args, the build command after it)."""
    if '--' not in argv:
        die("pr_libs_cache.py: missing '--' followed by the build command.")
    cut = argv.index('--')
    return argv[:cut], argv[cut + 1:]


def cache_args(args):
    """The selector (+ tag) shared by both cache scripts."""
    sel = ['--image', args.image] if args.image else ['--platform', args.platform]
    return sel + ['--tag', args.tag]


def run(cmd):
    """Run a subprocess, streaming its output; return the exit code."""
    return subprocess.run(cmd).returncode


def main(argv):
    ours, build_cmd = split_build_command(argv[1:])
    parser = argparse.ArgumentParser(description=__doc__)
    sel = parser.add_mutually_exclusive_group(required=True)
    sel.add_argument('--image', help='builder image reference')
    sel.add_argument('--platform', help='literal platform label')
    parser.add_argument('--tag', required=True, help='hashFiles content hash')
    parser.add_argument('--pr', default='',
                        help='PR number (empty for forks: no branch cache)')
    parser.add_argument('--ensure', action='store_true',
                        help='maybe-build mode: check existence (no download); '
                             'on miss build and HARD-push the branch cache. '
                             'Requires --pr (non-fork only).')
    args = parser.parse_args(ours)
    if not build_cmd:
        die("pr_libs_cache.py: no build command after '--'.")
    if args.ensure and not args.pr:
        die("pr_libs_cache.py --ensure requires --pr (non-fork only).")

    base = cache_args(args)
    branch = base + ['--pr', args.pr]
    verb = 'exists' if args.ensure else 'pull'

    if run([sys.executable, MAIN_CACHE, verb] + base) == 0:
        info("Libs present in/restored from the main GHCR cache.")
        return
    if args.pr and run([sys.executable, BRANCH_CACHE, verb] + branch) == 0:
        info("Libs present in/restored from the branch GHCR cache.")
        return

    info("Libs cache miss; building from source.")
    rc = run(build_cmd)
    if rc != 0:
        sys.exit(rc)

    if args.pr:
        info("Pushing freshly built libs to the branch GHCR cache.")
        push_rc = run([sys.executable, BRANCH_CACHE, 'push'] + branch)
        if push_rc != 0:
            if args.ensure:
                die("branch libs cache push failed.")
            info("warning: branch libs cache push failed; will rebuild next "
                 "push.")


if __name__ == '__main__':
    main(sys.argv)
