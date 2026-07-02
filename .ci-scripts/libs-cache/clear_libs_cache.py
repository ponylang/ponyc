#!/usr/bin/env python3
"""Delete the GHCR libs-cache packages and re-warm them.

The GitHub Actions cache had `gh cache delete` / a UI button; GHCR has no
equivalent, and because the artifact tag is the content hash there is no "touch
to expire" -- deletion is the only way to invalidate. This is the operational
escape hatch for "the cached libs might be bad / we want a guaranteed clean
slate": it deletes every `ponyc-libs-cache/*` package outright, then dispatches
the warmer (`update-lib-cache`) to rebuild and re-push everything (self-healing
via the warmer's build-on-miss).

Stdlib only. The REST plumbing (gh_request/paginate/encode) is shared via
`ghpackages.py`.

Auth needs BOTH tokens because each can only do the half its scope allows:
  - PONYLANG_MAIN_READ_PACKAGE_TOKEN (org-level PAT, `read:packages`) ENUMERATES
    the org's packages (200). The repo-scoped GITHUB_TOKEN cannot do an
    org-level list -- that endpoint returns 400 for it.
  - GITHUB_TOKEN (repo-scoped, `packages: write`) DELETES the packages (204) --
    they are this repo's packages. An org PAT is not the repo token and is not
    a package admin, so it gets 404 on the delete.
The re-warm dispatch uses GITHUB_TOKEN (`actions: write`).

Usage:
    clear_libs_cache.py [--prefix <p>] [--dry-run]

`--prefix` selects which package namespace to clear (default the cache namespace
`ponyc-libs-cache/`). When the prefix is outside the cache namespace (used once
to clean up the obsolete pre-namespacing `ponyc-libs-*` packages), the cache
namespace is force-excluded so this can never delete the live cache.
"""

import argparse
import sys

from common import die, info, require_env
from ghpackages import encode, gh_request, paginate

NAMESPACE = 'ponyc-libs-cache'
WARMER_WORKFLOW = 'update-lib-cache.yml'


def list_cache_packages(owner, prefix, token):
    names = []
    path = f'/orgs/{owner}/packages?package_type=container'
    for pkg in paginate(path, token):
        name = pkg['name']
        if not name.startswith(prefix):
            continue
        # Gate: a non-cache prefix (the one-time pre-namespacing cleanup) must
        # never sweep up the live cache namespace.
        if not prefix.startswith(NAMESPACE) and name.startswith(NAMESPACE + '/'):
            continue
        names.append(name)
    return names


def delete_package(owner, name, token):
    path = f'/orgs/{owner}/packages/container/{encode(name)}'
    status, _, body = gh_request('DELETE', path, token)
    if status not in (204, 404):
        text = body.decode('utf-8', errors='replace') if body else ''
        die(f"DELETE {path} returned {status}\n{text}")


def dispatch_warmer(owner, repo, token):
    path = f'/repos/{owner}/{repo}/actions/workflows/{WARMER_WORKFLOW}/dispatches'
    status, _, body = gh_request('POST', path, token, data={'ref': 'main'})
    if status != 204:
        text = body.decode('utf-8', errors='replace') if body else ''
        die(f"POST {path} returned {status} (expected 204)\n{text}")
    info(f"Dispatched {WARMER_WORKFLOW} on main.")


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', default=NAMESPACE + '/',
                        help='package-name prefix to clear')
    parser.add_argument('--dry-run', action='store_true',
                        help='list what would be deleted, delete nothing')
    args = parser.parse_args(argv[1:])

    repo_full = require_env('GITHUB_REPOSITORY')
    owner, repo = repo_full.split('/', 1)
    read_token = require_env('PONYLANG_MAIN_READ_PACKAGE_TOKEN')   # org enumeration
    write_token = require_env('GITHUB_TOKEN')           # delete + dispatch

    info(f"Finding container packages matching '{args.prefix}*' in {owner}...")
    names = list_cache_packages(owner, args.prefix, read_token)
    if not names:
        info("No matching packages found; nothing to delete.")
    else:
        info(f"Matched {len(names)} package(s):")
        for name in names:
            print(f"  {name}")

    if args.dry_run:
        info("Dry run: deleting nothing.")
        return

    for name in names:
        info(f"Deleting {name}...")
        delete_package(owner, name, write_token)

    dispatch_warmer(owner, repo, write_token)


if __name__ == '__main__':
    main(sys.argv)
