#!/usr/bin/env python3
"""Retention for the GHCR libs cache: keep the N newest versions per package.

Runs after the warmer on main. For every `ponyc-libs-cache/*` package it deletes
all but the N most-recently-created versions, bounding storage without ever
removing a package's current (newest) artifact.

Stdlib only. Auth needs BOTH tokens (each can only do the half its scope allows;
this is also why retention can't be snok/container-retention-policy, which takes
a single token):
  - PONYLANG_MAIN_READ_PACKAGE_TOKEN (org-level PAT, `read:packages`) ENUMERATES the
    org packages and lists each package's versions. The repo-scoped GITHUB_TOKEN
    can't do the org-level list (400).
  - GITHUB_TOKEN (repo-scoped, `packages: write`) DELETES versions (204). The org
    PAT can't (404 on these repo-scoped packages).

The REST plumbing (gh_request/paginate/encode) is shared via `ghpackages.py`.

Usage:
    prune_libs_cache.py [--keep N] [--dry-run]
"""

import argparse
import sys

from common import die, info, require_env
from ghpackages import encode, gh_request, paginate

NAMESPACE = 'ponyc-libs-cache'


def cache_packages(owner, token):
    path = f'/orgs/{owner}/packages?package_type=container'
    return [p['name'] for p in paginate(path, token)
            if p['name'].startswith(NAMESPACE + '/')]


def prune_package(owner, name, keep, read_token, write_token, dry_run):
    path = f'/orgs/{owner}/packages/container/{encode(name)}/versions'
    versions = sorted(paginate(path, read_token),
                      key=lambda v: v['created_at'], reverse=True)
    stale = versions[keep:]
    if not stale:
        return 0
    for v in stale:
        if dry_run:
            info(f"  would delete version {v['id']} ({v['created_at']})")
            continue
        del_path = f'/orgs/{owner}/packages/container/{encode(name)}/versions/{v["id"]}'
        status, _, body = gh_request('DELETE', del_path, write_token)
        if status not in (204, 404):
            text = body.decode('utf-8', errors='replace') if body else ''
            die(f"DELETE {del_path} returned {status}\n{text}")
    return len(stale)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--keep', type=int, default=2,
                        help='number of newest versions to keep per package')
    parser.add_argument('--dry-run', action='store_true',
                        help='list what would be deleted, delete nothing')
    args = parser.parse_args(argv[1:])
    if args.keep < 1:
        die("--keep must be at least 1 (never delete a package's last version)")

    owner = require_env('GITHUB_REPOSITORY').split('/')[0]
    read_token = require_env('PONYLANG_MAIN_READ_PACKAGE_TOKEN')   # org enumeration
    write_token = require_env('GITHUB_TOKEN')           # version deletes

    names = cache_packages(owner, read_token)
    info(f"Pruning {len(names)} package(s) to the {args.keep} newest version(s) each.")
    total = 0
    for name in names:
        deleted = prune_package(owner, name, args.keep, read_token, write_token, args.dry_run)
        if deleted:
            info(f"  {name}: removed {deleted} old version(s)")
            total += deleted
    info(f"Done. Removed {total} old version(s).")


if __name__ == '__main__':
    main(sys.argv)
