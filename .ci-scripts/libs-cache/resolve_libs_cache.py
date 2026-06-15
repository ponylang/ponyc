#!/usr/bin/env python3
"""Resolve the prebuilt LLVM `build/libs` for a non-PR CI job (consumer or warmer).

This is the orchestration the build/warmer jobs call instead of building LLVM
directly. It sequences the `oci_libs_cache.py` primitives around the build command
the workflow hands it after `--`; it does not reimplement any caching itself. It
is CI-only logic, so it lives here, in a script the workflows call, NOT in the
Makefile / make.ps1 (which are the developer-facing build files and know only how
to *build* libs). The PR jobs have their own orchestration (`pr_libs_cache.py`),
which also layers in the per-PR branch cache; this one only ever touches the main
`ponyc-libs-cache`.

Three modes:

  - consumer mode (default): `pull` the main cache -> hit -> libs restored into
    the working tree, done. miss -> run the build command. NO push. This is what
    every build job needs (the libs to build ponyc against) and is exactly the
    old `make libs-ci` behavior.
  - warmer mode (`--warm`): `exists` (a HEAD check, no download -- the warmer
    never needs the libs locally) -> hit -> done. miss -> run the build command,
    then `push` the result to the main cache. Only `--warm` pushes (consumer
    mode has no push path -- this is script-enforced); by workflow convention
    only the warmer passes `--warm`, so it stays the main cache's only writer.
  - require-cache-hit mode (`--require-cache-hit`): `pull` the main cache -> hit
    -> done. miss -> FAIL LOUDLY (no build). Takes no build command. For
    nice-to-have jobs (the stress tests) that must rely on a pre-warmed cache: a
    miss means the cache is empty for this hash (just cleared / inputs changed --
    it refills on the next warmer run) or the platform is missing from the
    warmer's matrix, and we want that surfaced as a red job, not masked by a
    silent multi-minute cold build.

The `--warm` name intentionally differs from `pr_libs_cache.py`'s `--ensure`:
that mode pushes the *branch* cache and hard-fails on a push error; this one
pushes the *main* cache. Distinct names so the two are not assumed interchangeable.

Stdlib only. Usage:
    # consumer (pull-or-build) / warmer (--warm: warm-and-push) -- build cmd required
    resolve_libs_cache.py [--warm] (--image <ref> | --platform <label>) \
        --tag <hash> -- <build command...>
    # require-cache-hit (pull-or-FAIL; stress tests) -- NO build command
    resolve_libs_cache.py --require-cache-hit (--image <ref> | --platform <label>) \
        --tag <hash>

For the consumer/warmer forms, everything after `--` is the build command, run
as-is on a cache miss (e.g. `make libs llvm_tools=false build_flags=-j4`, or on
Windows `pwsh -File make.ps1 -Command libs -LlvmTools false`). Auth for the
pull/exists/push uses GITHUB_TOKEN; the `--warm` push needs `packages: write`.
"""

import argparse
import os
import sys

from common import cache_args, die, info, run, split_build_command

SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
MAIN_CACHE = os.path.join(SCRIPTS_DIR, 'oci_libs_cache.py')


def main(argv):
    rest = argv[1:]
    parser = argparse.ArgumentParser(description=__doc__)
    sel = parser.add_mutually_exclusive_group(required=True)
    sel.add_argument('--image', help='builder image reference')
    sel.add_argument('--platform', help='literal platform label')
    parser.add_argument('--tag', required=True, help='hashFiles content hash')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--warm', action='store_true',
                      help='warmer mode: check existence (no download); on a '
                           'miss build and push the main cache. Default mode '
                           'pulls-or-builds and never pushes.')
    mode.add_argument('--require-cache-hit', action='store_true',
                      help='require-cache-hit mode: pull the main cache; on a '
                           'miss fail loudly instead of building. Takes no build '
                           'command. For nice-to-have jobs (stress tests) that '
                           'must rely on a pre-warmed cache.')

    # Choose the mode from the args BEFORE any `--`. We can't parse first (the
    # consumer/warm build command after `--` would break argparse), and we must
    # know the mode before deciding whether a `--` is even expected. Looking only
    # at the pre-`--` args means a build-command token can never be misread as
    # the mode flag; the sole residual assumption is that no selector/tag value
    # equals the exact string '--require-cache-hit' (none can -- selectors are
    # fixed image refs / labels and --tag is a content hash). require-cache-hit
    # takes NO build command and never builds.
    pre_dash = rest[:rest.index('--')] if '--' in rest else rest
    if '--require-cache-hit' in pre_dash:
        if '--' in rest:
            die("--require-cache-hit takes no build command (it never builds).")
        args = parser.parse_args(rest)
        base = cache_args(args)
        if run([sys.executable, MAIN_CACHE, 'pull'] + base) == 0:
            info("Libs restored from the main GHCR cache.")
            return
        sel_str = args.image or args.platform
        die(f"Libs cache MISS for require-cache-hit platform '{sel_str}' (tag "
            f"{args.tag}). Stress tests require a pre-warmed libs cache and "
            "never cold-build. Likely causes: (1) the cache was just cleared or "
            "an LLVM-determining input changed -- it refills on the next "
            "push-to-main run of update-lib-cache.yml; (2) this platform is "
            "missing from update-lib-cache.yml (the warmer). See the GHCR libs "
            "cache coupling in .github/workflows/AGENTS.md.")

    # consumer / warmer: the build command after `--` is mandatory.
    ours, build_cmd = split_build_command(rest)
    args = parser.parse_args(ours)
    if not build_cmd:
        die("no build command after '--'.")

    base = cache_args(args)
    verb = 'exists' if args.warm else 'pull'

    if run([sys.executable, MAIN_CACHE, verb] + base) == 0:
        info("Libs present in/restored from the main GHCR cache.")
        return

    info("Libs cache miss; building from source.")
    rc = run(build_cmd)
    if rc != 0:
        sys.exit(rc)

    if args.warm:
        info("Pushing freshly built libs to the main GHCR cache.")
        if run([sys.executable, MAIN_CACHE, 'push'] + base) != 0:
            die("libs cache push failed.")


if __name__ == '__main__':
    main(sys.argv)
