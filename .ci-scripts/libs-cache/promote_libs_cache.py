#!/usr/bin/env python3
"""Promote a branch libs-cache artifact into the main libs cache (same tag).

The warmer (`update-lib-cache.yml`, via `resolve_libs_cache.py --warm` or the BSD
jobs' host-side check) calls this on a main-cache MISS: instead of cold-building
LLVM, copy the artifact a PR or an ad-hoc tier dispatch already built and pushed to
the branch scratch cache (`branch_libs_cache.py` / `ponyc-branch-libs-cache`) into
the main cache (`oci_libs_cache.py` / `ponyc-libs-cache`).

This is the one script that spans both namespaces. The two package names are the
same `<platform>-<arch>` string under different namespace prefixes and share the
same content-hash tag, so the branch artifact is a valid main artifact -- the only
work is a registry-to-registry copy (`registry.copy`), which downloads the source
archive blob and re-uploads it under the main package with a fresh manifest. No
`build/libs` is needed on disk, so the warmer's BSD jobs can promote host-side
without booting their VM.

The caller checks `branch_libs_cache.py exists` first and only invokes this on a
hit; `registry.copy` still re-checks (and dies if the source is absent), so a
promote is safe to call defensively. A promote failure is non-fatal to the warmer:
it falls through to a normal build.

Stdlib only. Usage:
    promote_libs_cache.py (--image <ref> | --platform <label>) --tag <hash>
"""

import argparse
import sys

import branch_libs_cache
import oci_libs_cache
import registry


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    sel = parser.add_mutually_exclusive_group(required=True)
    sel.add_argument('--image', help='builder image reference')
    sel.add_argument('--platform', help='literal platform label')
    parser.add_argument('--tag', required=True, help='hashFiles content hash')
    args = parser.parse_args(argv[1:])

    # Source (branch) and destination (main) names: the same platform/arch suffix
    # under two namespace prefixes.
    src = branch_libs_cache.resolve_package(args)
    dst = oci_libs_cache.resolve_package(args)
    registry.copy(src, dst, args.tag)


if __name__ == '__main__':
    main(sys.argv)
