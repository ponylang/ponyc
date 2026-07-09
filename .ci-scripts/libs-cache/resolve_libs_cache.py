#!/usr/bin/env python3
"""Resolve the prebuilt LLVM `build/libs` for a non-PR build (consumer, warmer, or
require-cache-hit).

This orchestration sequences the `oci_libs_cache.py` / `branch_libs_cache.py` /
`promote_libs_cache.py` primitives around the build command it is handed after
`--`, instead of building LLVM directly; it does not reimplement any caching
itself. It is CI-only logic, so it lives here, in a script the workflows call, NOT
in the build command itself (`cmake -P lib/build-libs.cmake`), which only knows
how to *build* libs. PR builds have their own orchestration
(`pr_libs_cache.py`).

Modes:

  - consumer mode (default): try to `pull` the libs from the main cache. If they
    are there, done. If not, run the build command. It never pushes the main cache
    (only `--warm` does). With `--branch-cache` it also uses the branch scratch
    cache: when the main cache doesn't have the libs it tries the branch cache too,
    and after building it pushes the result to the branch cache (best effort). Pass
    `--branch-cache` for a run that might build an LLVM change worth keeping (so the
    build is saved for reuse); omit it for a run that only ever wants the main cache.
  - warmer mode (`--warm`): check whether the main cache already has the libs (a
    HEAD request, no download). If it does, done. If not, check the branch cache
    and, if it has them, `promote` (copy) them into the main cache instead of
    building. Only when neither cache has them (or the promote fails) does it build
    and `push` the main cache. `--warm` is the only mode that writes the main cache.
  - require-cache-hit mode (`--require-cache-hit`): try to `pull` from the main
    cache. Takes no build command -- it never builds. For a run that must rely on
    already-built libs rather than build them itself. Two modifiers set what happens
    when the libs aren't cached:
      * `--skip-on-miss`: write the `.libs-cache-miss` marker and exit 0 instead of
        failing. The caller skips its build/run steps when the marker is present, so
        the run passes with those steps skipped. For a run where the libs being
        absent is expected -- e.g. one that can overlap the cache being refilled.
      * `--branch-cache`: also try the branch cache before giving up -- the same
        main-then-branch lookup a consumer does. It only pulls here; it never builds
        or pushes.
    With neither modifier -- or with `--branch-cache` and neither cache has the libs
    -- the run fails with an error (no build).

`--branch-cache` is consumer or require-cache-hit; it is rejected with `--warm` (a
warmer never reads the branch cache as a consumer). The `--warm` name intentionally
differs from `pr_libs_cache.py`'s `--ensure`: that pushes the *branch* cache and
hard-fails on a push error; `--warm` pushes/promotes the *main* cache. Distinct
names so the two are not assumed interchangeable.

Stdlib only. Usage:
    # consumer (pull-or-build) -- build cmd required; --branch-cache to capture
    resolve_libs_cache.py [--branch-cache] (--image <ref> | --platform <label>) \
        --tag <hash> -- <build command...>
    # warmer (promote-or-build-and-push-main)
    resolve_libs_cache.py --warm (--image <ref> | --platform <label>) \
        --tag <hash> -- <build command...>
    # require-cache-hit (pull, or fail if not cached) -- NO build command.
    # --skip-on-miss: if not cached, skip the run instead of failing it;
    # --branch-cache:  also try the branch cache (it only pulls, never builds).
    resolve_libs_cache.py --require-cache-hit [--skip-on-miss | --branch-cache] \
        (--image <ref> | --platform <label>) --tag <hash>

For the consumer/warmer forms, everything after `--` is the build command, run
as-is on a cache miss (e.g.
`cmake -DJOBS=4 -P lib/build-libs.cmake`, or on Windows
`cmake -DPRESET=libs-windows-x86-64 -P lib/build-libs.cmake`). Auth
uses GITHUB_TOKEN; the main push (`--warm`) and the branch push (`--branch-cache`)
need `packages: write`.
"""

import argparse
import os
import sys
from pathlib import Path

from common import (BRANCH_CACHE, MAIN_CACHE, PROMOTE, cache_args, die, info, run,
                    split_build_command)

# A caller that passes --skip-on-miss gates its build/run steps on the ABSENCE of
# this marker (`if: hashFiles('.libs-cache-miss') == ''`), so when the libs are not
# cached we write it to signal "skip this run". It lives in the workspace root.
MISS_MARKER = '.libs-cache-miss'


def write_miss_marker():
    """Write the skip-on-miss marker into the workspace so the workflow's
    `hashFiles` gate sees it. We use GITHUB_WORKSPACE, falling back to cwd.

    The `or '.'` fallback is load-bearing when the resolve step runs inside
    `docker run -w <mounted-workspace>` WITHOUT GITHUB_WORKSPACE in the container
    env: the marker must land in cwd, which is that mount root -- where the
    host-side `hashFiles` looks. If that working directory ever stops being the
    workspace mount, the host gate can no longer see the marker."""
    (Path(os.environ.get('GITHUB_WORKSPACE') or '.') / MISS_MARKER).touch()


def main(argv):
    rest = argv[1:]
    parser = argparse.ArgumentParser(description=__doc__)
    sel = parser.add_mutually_exclusive_group(required=True)
    sel.add_argument('--image', help='builder image reference')
    sel.add_argument('--platform', help='literal platform label')
    parser.add_argument('--tag', required=True, help='hashFiles content hash')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--warm', action='store_true',
                      help='warmer mode: HEAD the main cache; on a miss promote a '
                           'branch artifact or build, then push the main cache. '
                           'Default mode pulls-or-builds and never pushes the main '
                           'cache.')
    mode.add_argument('--require-cache-hit', action='store_true',
                      help='require-cache-hit mode: pull from the main cache; if '
                           'the libs are not cached, fail with an error instead of '
                           'building. Takes no build command. For a run that must '
                           'rely on already-built libs.')
    parser.add_argument('--branch-cache', action='store_true',
                        help='consumer or require-cache-hit mode: also use the '
                             'branch scratch cache. Consumer: pull from it when the '
                             'main cache does not have the libs, and push to it '
                             'after a build (best effort; needs packages: write). '
                             'require-cache-hit: pull from it before failing (it '
                             'only pulls, never builds).')
    parser.add_argument('--skip-on-miss', action='store_true',
                        help='require-cache-hit mode only: if the libs are not '
                             'cached, write the .libs-cache-miss marker and exit 0 '
                             'instead of failing, so the workflow skips its '
                             'build/run steps. For a run where the libs being '
                             'absent is expected.')

    # Choose the mode from the args BEFORE any `--`. We can't parse first (the
    # consumer/warm build command after `--` would break argparse), and we must
    # know the mode before deciding whether a `--` is even expected. Looking only
    # at the pre-`--` args means a build-command token can never be misread as a
    # mode flag; the sole residual assumption is that no selector/tag value equals
    # the exact string '--require-cache-hit' (none can). require-cache-hit takes NO
    # build command and never builds.
    pre_dash = rest[:rest.index('--')] if '--' in rest else rest
    if '--require-cache-hit' in pre_dash:
        if '--' in rest:
            die("--require-cache-hit takes no build command (it never builds).")
        args = parser.parse_args(rest)
        base = cache_args(args)
        if run([sys.executable, MAIN_CACHE, 'pull'] + base) == 0:
            info("Libs restored from the main GHCR cache.")
            return
        # When the main cache does not have the libs, --branch-cache checks the
        # branch cache too -- the same main-then-branch lookup a consumer does. It
        # only pulls here.
        if (args.branch_cache
                and run([sys.executable, BRANCH_CACHE, 'pull'] + base) == 0):
            info("Libs restored from the branch GHCR cache.")
            return
        sel_str = args.image or args.platform
        # --skip-on-miss treats the libs being absent as expected and skips
        # quietly; without it -- or with --branch-cache and neither cache has the
        # libs -- the run fails with an error.
        if args.skip_on_miss:
            write_miss_marker()
            info(f"Libs not cached for '{sel_str}' (tag {args.tag}); skipping this "
                 "run (wrote .libs-cache-miss). Expected when a run overlaps the "
                 "cache being refilled. See .ci-scripts/libs-cache/README.md.")
            return
        die(f"Libs not cached for require-cache-hit platform '{sel_str}' (tag "
            f"{args.tag}). This job needs already-built libs and never builds them "
            "itself. Likely causes: (1) the cache was just cleared or an "
            "LLVM-determining input changed; (2) this platform is not covered by "
            "whatever fills the cache. See .ci-scripts/libs-cache/README.md.")

    # consumer / warmer: the build command after `--` is mandatory.
    ours, build_cmd = split_build_command(rest)
    args = parser.parse_args(ours)
    if args.skip_on_miss:
        die("--skip-on-miss is only valid with --require-cache-hit.")
    if not build_cmd:
        die("no build command after '--'.")
    if args.warm and args.branch_cache:
        die("--branch-cache is not valid with --warm.")

    base = cache_args(args)

    if args.warm:
        if run([sys.executable, MAIN_CACHE, 'exists'] + base) == 0:
            info("Libs present in the main GHCR cache.")
            return
        if run([sys.executable, BRANCH_CACHE, 'exists'] + base) == 0:
            info("Branch cache hit; promoting it into the main cache.")
            if run([sys.executable, PROMOTE] + base) == 0:
                info("Promoted branch libs into the main GHCR cache.")
                return
            info("warning: promote failed; building from source instead.")
        else:
            info("Libs cache miss; building from source.")
        rc = run(build_cmd)
        if rc != 0:
            sys.exit(rc)
        info("Pushing freshly built libs to the main GHCR cache.")
        if run([sys.executable, MAIN_CACHE, 'push'] + base) != 0:
            die("libs cache push failed.")
        return

    # consumer (default), optionally participating in the branch cache.
    if run([sys.executable, MAIN_CACHE, 'pull'] + base) == 0:
        info("Libs restored from the main GHCR cache.")
        return
    if args.branch_cache and run([sys.executable, BRANCH_CACHE, 'pull'] + base) == 0:
        info("Libs restored from the branch GHCR cache.")
        return

    info("Libs cache miss; building from source.")
    rc = run(build_cmd)
    if rc != 0:
        sys.exit(rc)

    if args.branch_cache:
        info("Pushing freshly built libs to the branch GHCR cache.")
        if run([sys.executable, BRANCH_CACHE, 'push'] + base) != 0:
            info("warning: branch libs cache push failed; will rebuild next run.")


if __name__ == '__main__':
    main(sys.argv)
