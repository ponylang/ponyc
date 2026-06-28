#!/usr/bin/env python3
"""Resolve the scratch copy of the libs cache (`ponyc-branch-libs-cache`) and run
a push/pull/exists/package-name against it.

This is the BRANCH libs cache: a separate, short-lived cache from the main libs
cache (`oci_libs_cache.py` / `ponyc-libs-cache`). It exists so a build that misses
the main cache because it changed an LLVM-determining input -- a non-fork PR, or an
ad-hoc `workflow_dispatch` of tier2/weekly on a branch -- builds LLVM once
and reuses that build on later runs instead of cold-building every time. The warmer
also reads it: on a main-cache miss it promotes a matching branch artifact into the
main cache instead of cold-building (`promote_libs_cache.py`).

It does NOT touch or read the main libs cache. The main cache is the source of
truth: a consumer pulls the main cache first and only falls back to this branch
cache on a miss. Retention is age-based (`prune_branch_libs_cache.py`), not the main
cache's keep-newest policy. The registry-v2 plumbing is shared with the main cache
via `registry.py`.

Package naming (assembled in one place, `cache_package`):

    ponyc-branch-libs-cache/<platform>-<arch>

  - `ponyc-branch-libs-cache/` is a path namespace, kept separate from both the
    main cache and distributable containers, so a `ponyc-branch-libs-cache/*`
    glob can only ever sweep up branch-cache artifacts.
  - `<platform>` is the builder image's name + date (`registry.derive_platform`),
    or a literal label for the non-container platforms (macOS/Windows/BSD).
  - `<arch>` is read from the build machine (`platform.machine()`) and normalized
    to one canonical spelling per ISA by `cache_arch.canonical`; the multi-arch
    builder images build the same reference on x86-64 and arm64, so arch must be
    in the name or the two would clobber each other.

The name is **tag-addressable**: the package identity is just
`<platform>-<arch>` and the version is the `hashFiles(...)` content hash -- the
same input keying as the main cache, and exactly the main cache's name under a
different namespace prefix. So the warmer can construct this name from the platform
it is building for and check it with one HEAD; no per-PR partitioning is needed.
The tag (not who built it) is what prevents a stale artifact from being served: a
branch that changes any LLVM-determining input gets a different tag (or platform),
so two builds that share a tag share identical content, and a build at a different
tag is a different version -- exact hit or miss. Two PRs (or a PR and a tier
dispatch) that produce the same tag share the one package, which is free dedup,
not a hazard.

Usage:
    branch_libs_cache.py package-name (--image <ref> | --platform <lbl>)
    branch_libs_cache.py exists --tag <hash> (--image <ref> | --platform <lbl>)
    branch_libs_cache.py pull --tag <hash> (--image <ref> | --platform <lbl>)
    branch_libs_cache.py push --tag <hash> (--image <ref> | --platform <lbl>)

`exists` reports whether the artifact is cached (exit 0 present / 1 absent)
without downloading the blob -- the cheap check a maybe-build job or the warmer
gates on. `pull` restores the artifact, exiting non-zero on a miss so the caller
can fall back to building. `push` uploads the blob before the manifest; auth uses
GITHUB_TOKEN (needs `packages: write`, which only non-fork PR runs and
`workflow_dispatch`/`schedule` runs get).

Stdlib only so no pip install is required on any CI runner.
"""

import platform
import sys

import cache_arch
import registry
from common import die

NAMESPACE = 'ponyc-branch-libs-cache'


def cache_package(platform_id):
    """Assemble the full GHCR package name for a platform/arch.

    The one place namespace and architecture are attached, so a pull and a push
    for the same platform always agree on the name. This is exactly the main
    cache's package name under a different namespace prefix, which is what lets the
    warmer discover a promotable artifact by construction rather than by search.
    """
    try:
        arch = cache_arch.canonical(platform.machine())
    except ValueError as e:
        die(str(e))
    return f'{NAMESPACE}/{platform_id}-{arch}'


def resolve_package(args):
    if args.image:
        platform_id = registry.derive_platform(args.image)
    else:
        platform_id = args.platform
    return cache_package(platform_id)


def main(argv):
    args = registry.build_parser(__doc__).parse_args(argv[1:])
    registry.dispatch(args, resolve_package(args))


if __name__ == '__main__':
    main(sys.argv)
