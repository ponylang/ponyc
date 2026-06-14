#!/usr/bin/env python3
"""Resolve a per-PR scratch copy of the libs cache (`ponyc-branch-libs-cache`)
and run a push/pull/exists/package-name against it.

This is the BRANCH libs cache: a separate, short-lived cache from the main libs
cache (`oci_libs_cache.py` / `ponyc-libs-cache`). It exists so a non-fork PR that
changes an LLVM-determining input builds LLVM once on its first push and reuses
that build on later pushes, instead of cold-building every push. Each PR gets its
own package, so one PR's scratch can never be served to another PR or to main.

It does NOT touch or read the main libs cache. The main cache is the source of
truth: a job pulls the main cache first and only falls back to this branch cache
on a miss (the PR-job flow is orchestrated by `pr_libs_cache.py`). Retention is
age-based (`prune_branch_libs_cache.py`), not the main cache's keep-newest policy.
The registry-v2 plumbing is shared with the main cache via `registry.py`.

Package naming (assembled in one place, `cache_package`):

    ponyc-branch-libs-cache/<platform>-<arch>-pr<N>

  - `ponyc-branch-libs-cache/` is a path namespace, kept separate from both the
    main cache and distributable containers, so a `ponyc-branch-libs-cache/*`
    glob can only ever sweep up branch-cache artifacts.
  - `<platform>` is the builder image's name + date (`registry.derive_platform`),
    or a literal label for the non-container platforms (macOS/Windows/BSD).
  - `<arch>` is read from the build machine (`platform.machine()`) and normalized
    to one canonical spelling per ISA by `cache_arch.canonical`; the multi-arch
    builder images build the same reference on x86-64 and arm64, so arch must be
    in the name or the two would clobber each other.
  - `<N>` is the pull-request number, so each PR's scratch is isolated.

The tag is the `hashFiles(...)` content hash -- the same input keying as the main
cache, so a branch that changes any LLVM-determining input gets a different tag
(or platform), and a stale artifact can never be served: exact hit or miss.

Usage:
    branch_libs_cache.py package-name --pr <N> (--image <ref> | --platform <lbl>)
    branch_libs_cache.py exists --tag <hash> --pr <N> (--image <ref> | --platform <lbl>)
    branch_libs_cache.py pull --tag <hash> --pr <N> (--image <ref> | --platform <lbl>)
    branch_libs_cache.py push --tag <hash> --pr <N> (--image <ref> | --platform <lbl>)

`exists` reports whether the artifact is cached (exit 0 present / 1 absent)
without downloading the blob -- the cheap check a maybe-build job gates on.
`pull` restores the artifact, exiting non-zero on a miss so the caller can fall
back to building. `push` uploads the blob before the manifest; auth uses
GITHUB_TOKEN (needs `packages: write`, which only non-fork PR runs get).

Stdlib only so no pip install is required on any CI runner.
"""

import platform
import sys

import cache_arch
import registry
from common import die

NAMESPACE = 'ponyc-branch-libs-cache'


def cache_package(platform_id, pr):
    """Assemble the full GHCR package name for a platform/arch/PR.

    The one place namespace, architecture, and PR number are attached, so a pull
    and a push for the same PR on the same platform always agree on the name.
    """
    try:
        arch = cache_arch.canonical(platform.machine())
    except ValueError as e:
        die(str(e))
    return f'{NAMESPACE}/{platform_id}-{arch}-pr{pr}'


def resolve_package(args):
    if args.image:
        platform_id = registry.derive_platform(args.image)
    else:
        platform_id = args.platform
    return cache_package(platform_id, args.pr)


def main(argv):
    args = registry.build_parser(__doc__, with_pr=True).parse_args(argv[1:])
    registry.dispatch(args, resolve_package(args))


if __name__ == '__main__':
    main(sys.argv)
