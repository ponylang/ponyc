#!/usr/bin/env python3
"""Resolve the prebuilt LLVM `build/libs` for a non-PR CI job (consumer or warmer).

This is the orchestration the build/warmer jobs call instead of building LLVM
directly. It sequences the `oci_libs_cache.py` / `branch_libs_cache.py` /
`promote_libs_cache.py` primitives around the build command the workflow hands it
after `--`; it does not reimplement any caching itself. It is CI-only logic, so it
lives here, in a script the workflows call, NOT in the Makefile / make.ps1 (which
are the developer-facing build files and know only how to *build* libs). The PR
jobs have their own orchestration (`pr_libs_cache.py`).

Modes:

  - consumer mode (default): `pull` the main cache -> hit -> libs restored, done.
    miss -> run the build command. NO main push (the warmer is the main cache's
    only writer). With `--branch-cache` it also participates in the branch scratch
    cache: on a main miss it `pull`s the branch cache (reuse across runs of the
    same changed-libs branch) and, after a build, `push`es the branch cache
    best-effort. tier2/tier3/weekly pass `--branch-cache` so an ad-hoc
    `workflow_dispatch` validating an LLVM change captures its build instead of
    discarding it; release/nightly use plain consumer mode (main-only).
  - warmer mode (`--warm`): `exists` the main cache (a HEAD, no download) -> hit ->
    done. miss -> `exists` the branch cache and, on a hit, `promote` it into the
    main cache (a registry copy -- reuses the build a PR or tier dispatch already
    made) instead of cold-building. Only on a branch miss (or a failed promote) does
    it build and `push` the main cache. `--warm` is the only path that writes the
    main cache; by workflow convention only the warmer passes it.
  - require-cache-hit mode (`--require-cache-hit`): `pull` the main cache -> hit ->
    done. Takes no build command (it never builds). For nice-to-have jobs (the
    stress tests) that must rely on a pre-warmed cache. Two modifiers tune the miss
    behavior:
      * `--skip-on-miss`: on a miss, write the `.libs-cache-miss` marker and exit 0
        (the workflow gates its build/run steps on the marker's ABSENCE, so the job
        goes green with those steps skipped). The scheduled stress loop uses this --
        it runs at times that legitimately overlap an empty/refilling cache, so a
        miss is expected, not a failure.
      * `--branch-cache`: on a main miss, also `pull` the branch cache before giving
        up -- the same main->branch resolution a PR consumer uses. A manual
        `workflow_dispatch` stress run on a branch uses this so it finds the libs
        that branch already built for the target. Pull-only here (never builds, so
        never pushes).
    With neither modifier -- or with `--branch-cache` and BOTH caches missing -- a
    miss FAILS LOUDLY (no build).

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
    # require-cache-hit (pull-or-FAIL; stress tests) -- NO build command.
    # --skip-on-miss: green-skip on miss (scheduled loop);
    # --branch-cache:  also pull the branch cache on a main miss (manual branch run).
    resolve_libs_cache.py --require-cache-hit [--skip-on-miss | --branch-cache] \
        (--image <ref> | --platform <label>) --tag <hash>

For the consumer/warmer forms, everything after `--` is the build command, run
as-is on a cache miss (e.g. `make libs llvm_tools=false build_flags=-j4`, or on
Windows `pwsh -File make.ps1 -Command libs -LlvmTools false`). Auth uses
GITHUB_TOKEN; the main push (`--warm`) and the branch push (`--branch-cache`) need
`packages: write`.
"""

import argparse
import os
import sys
from pathlib import Path

from common import (BRANCH_CACHE, MAIN_CACHE, PROMOTE, cache_args, die, info, run,
                    split_build_command)

# The stress workflows gate their build/run steps on the ABSENCE of this marker
# (`if: hashFiles('.libs-cache-miss') == ''`), so on a `--skip-on-miss` miss we
# write it to signal "skip this run". It lives in the workspace root.
MISS_MARKER = '.libs-cache-miss'


def write_miss_marker():
    """Write the skip-on-miss marker into the workspace so the workflow's
    `hashFiles` gate sees it. We use GITHUB_WORKSPACE, falling back to cwd.

    The `or '.'` fallback is load-bearing for the arm64-linux stress job: its
    resolve step runs inside `docker run -w /home/pony/project` (the mounted
    workspace) WITHOUT GITHUB_WORKSPACE in the container env, so the marker must
    land in cwd, which is that mount root -- where the host-side `hashFiles`
    looks. If that job's working directory ever stops being the workspace mount,
    the host gate can no longer see the marker."""
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
                      help='require-cache-hit mode: pull the main cache; on a miss '
                           'fail loudly instead of building. Takes no build '
                           'command. For nice-to-have jobs (stress tests).')
    parser.add_argument('--branch-cache', action='store_true',
                        help='consumer or require-cache-hit mode: also participate '
                             'in the branch scratch cache. Consumer: pull it for '
                             'reuse on a main miss and push it best-effort after a '
                             'build (needs packages: write). require-cache-hit: pull '
                             'it on a main miss before failing (pull-only).')
    parser.add_argument('--skip-on-miss', action='store_true',
                        help='require-cache-hit mode only: on a miss, write the '
                             '.libs-cache-miss marker and exit 0 instead of failing '
                             'loudly, so the workflow skips its build/run steps. For '
                             'the scheduled stress loop, where a miss is expected.')

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
        # On a main miss, a manual branch run (--branch-cache) checks the branch
        # cache too -- the same main->branch resolution a PR consumer uses, so it
        # finds the libs that branch already built for this target. Pull-only.
        if (args.branch_cache
                and run([sys.executable, BRANCH_CACHE, 'pull'] + base) == 0):
            info("Libs restored from the branch GHCR cache.")
            return
        sel_str = args.image or args.platform
        # The scheduled stress loop (--skip-on-miss) treats a miss as expected and
        # skips quietly; without it -- or with --branch-cache and both caches empty
        # -- a miss fails loudly.
        if args.skip_on_miss:
            write_miss_marker()
            info(f"Libs cache miss for '{sel_str}' (tag {args.tag}); skipping this "
                 "stress run (wrote .libs-cache-miss). Expected when the continuous "
                 "stress loop overlaps an empty or refilling cache. See the GHCR "
                 "libs cache coupling in .github/workflows/AGENTS.md.")
            return
        die(f"Libs cache MISS for require-cache-hit platform '{sel_str}' (tag "
            f"{args.tag}). Stress tests require a pre-warmed libs cache and never "
            "cold-build. Likely causes: (1) the cache was just cleared or an "
            "LLVM-determining input changed -- it refills on the next "
            "push-to-main run of update-lib-cache.yml; (2) this platform is "
            "missing from update-lib-cache.yml (the warmer). See the GHCR libs "
            "cache coupling in .github/workflows/AGENTS.md.")

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
