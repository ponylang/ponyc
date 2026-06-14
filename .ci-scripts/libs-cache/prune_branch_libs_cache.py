#!/usr/bin/env python3
"""Age-based retention for the branch libs cache (`ponyc-branch-libs-cache/*`).

The branch libs cache (`branch_libs_cache.py`) holds a per-PR scratch copy of the
prebuilt LLVM so a non-fork PR doesn't rebuild LLVM on every push. Each PR is its
own package, so a closed/abandoned PR's package would otherwise sit in GHCR
forever. This sweep, run daily, deletes anything older than the cutoff: stale
versions individually, and a package outright once all of its versions are stale.

This is the branch cache's OWN retention -- deliberately different from the main
cache's `prune_libs_cache.py` (which keeps the N newest versions per package and
never deletes a package). It only ever enumerates and deletes within
`ponyc-branch-libs-cache/`, so it cannot touch the main `ponyc-libs-cache`.

Stdlib only. Auth needs BOTH tokens (each can only do the half its scope allows):
  - PONYLANG_MAIN_READ_PACKAGE_TOKEN (org-level PAT, `read:packages`) ENUMERATES
    the org packages and lists each package's versions. The repo-scoped
    GITHUB_TOKEN can't do the org-level list (400).
  - GITHUB_TOKEN (repo-scoped, `packages: write`) DELETES versions and packages
    (204). The org PAT can't (404 on these repo-scoped packages).

The REST plumbing (gh_request/paginate/encode) is shared via `ghpackages.py`.

Usage:
    prune_branch_libs_cache.py [--max-age-days N] [--dry-run]
"""

import argparse
import datetime
import sys

from common import die, info, require_env
from ghpackages import encode, gh_request, paginate

NAMESPACE = 'ponyc-branch-libs-cache'


def parse_time(value):
    # GitHub timestamps are ISO 8601 ending in 'Z'; normalize for fromisoformat.
    return datetime.datetime.fromisoformat(value.replace('Z', '+00:00'))


def branch_packages(owner, token):
    path = f'/orgs/{owner}/packages?package_type=container'
    return [p['name'] for p in paginate(path, token)
            if p['name'].startswith(NAMESPACE + '/')]


def delete_version(owner, name, version_id, token):
    path = (f'/orgs/{owner}/packages/container/{encode(name)}'
            f'/versions/{version_id}')
    status, _, body = gh_request('DELETE', path, token)
    if status not in (204, 404):
        text = body.decode('utf-8', errors='replace') if body else ''
        die(f"DELETE {path} returned {status}\n{text}")


def delete_package(owner, name, token):
    path = f'/orgs/{owner}/packages/container/{encode(name)}'
    status, _, body = gh_request('DELETE', path, token)
    if status not in (204, 404):
        text = body.decode('utf-8', errors='replace') if body else ''
        die(f"DELETE {path} returned {status}\n{text}")


def prune_package(owner, name, cutoff, read_token, write_token, dry_run):
    path = f'/orgs/{owner}/packages/container/{encode(name)}/versions'
    versions = list(paginate(path, read_token))
    stale = [v for v in versions if parse_time(v['created_at']) < cutoff]
    if not stale:
        return 0
    # Every version is stale -> drop the whole package in one call. This is the
    # closed/abandoned-PR reclaim: a package nobody has pushed to in the window.
    # Benign TOCTOU: if a still-open PR pushes a fresh version between the list
    # above and this delete, that version is dropped too -> a single cache miss
    # and one rebuild on its next push, which recreates the package. Acceptable
    # for a best-effort cache; not worth a guard.
    if len(stale) == len(versions):
        if dry_run:
            info(f"  would delete package {name} ({len(versions)} version(s))")
        else:
            delete_package(owner, name, write_token)
        return len(versions)
    for v in stale:
        if dry_run:
            info(f"  would delete version {v['id']} ({v['created_at']})")
        else:
            delete_version(owner, name, v['id'], write_token)
    return len(stale)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--max-age-days', type=int, default=14,
                        help='delete anything older than this many days')
    parser.add_argument('--dry-run', action='store_true',
                        help='list what would be deleted, delete nothing')
    args = parser.parse_args(argv[1:])
    if args.max_age_days < 1:
        die("--max-age-days must be at least 1")

    owner = require_env('GITHUB_REPOSITORY').split('/')[0]
    read_token = require_env('PONYLANG_MAIN_READ_PACKAGE_TOKEN')  # enumeration
    write_token = require_env('GITHUB_TOKEN')                     # deletes

    now = datetime.datetime.now(datetime.timezone.utc)
    cutoff = now - datetime.timedelta(days=args.max_age_days)
    names = branch_packages(owner, read_token)
    info(f"Pruning {len(names)} branch-cache package(s); cutoff "
         f"{cutoff.isoformat()} ({args.max_age_days} days).")
    total = 0
    for name in names:
        deleted = prune_package(owner, name, cutoff, read_token, write_token,
                                args.dry_run)
        if deleted:
            info(f"  {name}: removed {deleted} stale item(s)")
            total += deleted
    info(f"Done. Removed {total} stale item(s).")


if __name__ == '__main__':
    main(sys.argv)
