#!/usr/bin/env python3
"""Resolve the MAIN libs cache (`ponyc-libs-cache`) package and run a
push/pull/exists/package-name against it.

Building the vendored LLVM is slow, so CI caches the built `build/libs` (plus
`lib/llvm/src/compiler-rt/lib/builtins`) keyed on the inputs that determine the
build. This is the entry point for the main cache: it assembles the GHCR package
name and hands it to the shared registry-v2 client in `registry.py`.

Package naming (the full GHCR package name is assembled in one place,
`cache_package`):

    ponyc-libs-cache/<platform>-<arch>

  - `ponyc-libs-cache/` is a path namespace, so the cache packages sit apart
    from distributable containers (and a `ponyc-libs-cache/*` glob can never
    sweep up an unrelated container).
  - `<platform>` is the builder image's name + date (`registry.derive_platform`),
    or a literal label for the non-container platforms (macOS/Windows/BSD).
  - `<arch>` is read from the machine doing the build (`platform.machine()`) and
    normalized to one canonical spelling per ISA by `cache_arch.canonical` (so a
    FreeBSD `amd64` build and a Linux `x86_64` build of the same artifact share a
    package). The alpine and ubuntu26.04 builder images are multi-arch: the same
    image reference is built on both x86-64 and arm64 runners, so the arch must be
    in the package name or the two arches would clobber each other's artifact.

The tag is the `hashFiles(...)` content hash. `package:tag` is keyed on the same
inputs as the old `actions/cache` key: a branch that changes any LLVM-determining
input gets a different tag (files) or platform (builder image), so a stale
artifact can never be served -- the only outcomes are an exact hit or a miss.

Usage:
    oci_libs_cache.py package-name (--image <ref> | --platform <label>)
    oci_libs_cache.py exists --tag <hash> (--image <ref> | --platform <label>)
    oci_libs_cache.py pull --tag <hash> (--image <ref> | --platform <label>)
    oci_libs_cache.py push --tag <hash> (--image <ref> | --platform <label>)

`package-name` prints the full package name (for debugging). `exists` reports
whether the artifact is cached (exit 0 present / 1 absent) without downloading
the blob -- the cheap check a maybe-build job gates on. `pull` restores the
artifact into the working tree, exiting non-zero on a miss so the caller can fall
back to building. `push` (warmer only) uploads the blob before the manifest;
auth uses GITHUB_TOKEN (needs `packages: write`).

Stdlib only so no pip install is required on any CI runner.
"""

import platform
import sys

import cache_arch
import registry
from common import die

NAMESPACE = 'ponyc-libs-cache'


def cache_package(platform_id):
    """Assemble the full GHCR package name from a platform identity.

    The one place the namespace and architecture are attached, so the warmer and
    every consumer always agree on the name for a given platform on a given arch.
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
