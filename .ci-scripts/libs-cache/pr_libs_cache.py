#!/usr/bin/env python3
"""Resolve the prebuilt LLVM `build/libs` for a PR build.

A build that changes an LLVM-determining input misses the main cache on every push
until that change reaches main (the main cache is filled from main).
So, given a token that can write the branch cache, this orchestration also uses the
branch scratch cache (`branch_libs_cache.py`): build LLVM once on the first push
that misses, reuse it on later pushes.

This is the orchestration that ties the two caches together for a PR build. It is
CI-only logic -- it pushes scratch artifacts -- so it lives here, in a script the
workflow calls, NOT in the build command itself (`cmake -P lib/build-libs.cmake`),
which only knows how to *build* libs. It does not reimplement any caching itself:
it sequences the existing `oci_libs_cache.py` and `branch_libs_cache.py` primitives
and shells out to the build command it is handed.

Flow (per platform, per job):
  1. check the main cache              -> hit -> done, no build, no push.
  2. (if --branch-cache) check the branch -> hit -> done, no build, no push.
  3. miss -> run the build command -> (if --branch-cache) push to the branch cache.

`--branch-cache` enables the branch scratch cache: read it when the main cache
misses, and push to it after a build. The push needs `packages: write`, so pass the
flag only on a run that has it; omit it and the run just pulls the main cache or
builds -- no branch read, no push. The branch cache is tag-addressable
(`branch_libs_cache.py`): the platform+arch+tag fully name the artifact, so the flag
carries no identifier -- it is just on or off.

Two modes select how "check" and "push failure" behave:

  - consumer mode (default): step 1/2 `pull` (download the blob into the working
    tree on a hit) and the branch push is best-effort -- the libs are already
    built, so a cache-write hiccup degrades to "rebuild next push" rather than
    failing the job.
  - ensure mode (`--ensure`): step 1/2 `exists` (no download -- this job only needs
    to know whether to build, not the blob itself, so a hit is seconds) and a
    branch push failure HARD-fails the job. Use it for a job whose purpose is to
    guarantee the branch cache is populated so later consumers can rely on a hit; a
    silent push failure would leave each of them to cold-build. `--ensure` requires
    `--branch-cache`.

The sibling `resolve_libs_cache.py` (the consumer/warmer orchestration for non-PR
builds) spells its main-cache build-and-push mode `--warm`, not these flags, on
purpose: it pushes the *main* cache, where a push failure is fatal differently. Its
own `--branch-cache` consumer flag matches this one (participate in the branch
cache), but the warmer/main-push path is distinct -- don't assume the flags
interchange.

A main-cache hit short-circuits before any push, so this orchestration never writes
the main cache.

Stdlib only. Usage:
    pr_libs_cache.py [--ensure] [--branch-cache] (--image <ref> | --platform <lbl>) \
        --tag <hash> -- <build command...>

Everything after `--` is the build command, run as-is on a cache miss (e.g.
`cmake -DTOOLS=false -DJOBS=4 -P lib/build-libs.cmake`, or on Windows
`cmake -DPRESET=libs-windows-x86-64 -DTOOLS=false -P lib/build-libs.cmake`). Auth
for the pulls/pushes uses GITHUB_TOKEN; the branch push needs `packages: write`.
"""

import argparse
import sys

from common import (BRANCH_CACHE, MAIN_CACHE, cache_args, die, info, run,
                    split_build_command)


def main(argv):
    ours, build_cmd = split_build_command(argv[1:])
    parser = argparse.ArgumentParser(description=__doc__)
    sel = parser.add_mutually_exclusive_group(required=True)
    sel.add_argument('--image', help='builder image reference')
    sel.add_argument('--platform', help='literal platform label')
    parser.add_argument('--tag', required=True, help='hashFiles content hash')
    parser.add_argument('--branch-cache', action='store_true',
                        help='use the branch scratch cache: read it on a '
                             'main-cache miss, push to it after a build. The push '
                             'needs packages: write. Omit it to only pull main or '
                             'build.')
    parser.add_argument('--ensure', action='store_true',
                        help='ensure mode: check existence (no download); on a miss '
                             'build and HARD-push the branch cache, failing the job '
                             'if that push fails. Requires --branch-cache.')
    args = parser.parse_args(ours)
    if not build_cmd:
        die("no build command after '--'.")
    if args.ensure and not args.branch_cache:
        die("--ensure requires --branch-cache.")

    base = cache_args(args)
    verb = 'exists' if args.ensure else 'pull'

    if run([sys.executable, MAIN_CACHE, verb] + base) == 0:
        info("Libs present in/restored from the main GHCR cache.")
        return
    if args.branch_cache and run([sys.executable, BRANCH_CACHE, verb] + base) == 0:
        info("Libs present in/restored from the branch GHCR cache.")
        return

    info("Libs cache miss; building from source.")
    rc = run(build_cmd)
    if rc != 0:
        sys.exit(rc)

    if args.branch_cache:
        info("Pushing freshly built libs to the branch GHCR cache.")
        push_rc = run([sys.executable, BRANCH_CACHE, 'push'] + base)
        if push_rc != 0:
            if args.ensure:
                die("branch libs cache push failed.")
            info("warning: branch libs cache push failed; will rebuild next "
                 "push.")


if __name__ == '__main__':
    main(sys.argv)
