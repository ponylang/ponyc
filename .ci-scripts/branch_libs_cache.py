#!/usr/bin/env python3
"""Store and retrieve a per-PR scratch copy of the prebuilt LLVM `build/libs`.

This is the BRANCH libs cache: a separate, short-lived cache from the main libs
cache (`oci_libs_cache.py` / `ponyc-libs-cache`). It exists so a non-fork PR that
changes an LLVM-determining input builds LLVM once on its first push and reuses
that build on later pushes, instead of cold-building every push. Each PR gets its
own package, so one PR's scratch can never be served to another PR or to main.

It does NOT touch, import, or read the main libs cache. The main cache is the
source of truth: a job pulls the main cache first and only falls back to this
branch cache on a miss (the PR-job flow is orchestrated by `pr_libs_cache.py`).
Retention is age-based (`prune_branch_libs_cache.py`), not the main cache's
keep-newest policy.

Stdlib only so no pip install is required on any CI runner.

Package naming (assembled in one place, `cache_package`):

    ponyc-branch-libs-cache/<platform>-<arch>-pr<N>

  - `ponyc-branch-libs-cache/` is a path namespace, kept separate from both the
    main cache and distributable containers, so a `ponyc-branch-libs-cache/*`
    glob can only ever sweep up branch-cache artifacts.
  - `<platform>` is the builder image's name + date (`derive_platform`), or a
    literal label for the non-container platforms (macOS/Windows/BSD).
  - `<arch>` is read from the build machine (`platform.machine()`); the multi-arch
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
"""

import argparse
import base64
import datetime
import hashlib
import io
import json
import os
import platform
import re
import sys
import tarfile
import urllib.error
import urllib.parse
import urllib.request

ENDC = '\033[0m'
ERROR = '\033[31m'
INFO = '\033[34m'

REGISTRY = 'https://ghcr.io'
NAMESPACE = 'ponyc-branch-libs-cache'
REQUEST_TIMEOUT = 600

ARTIFACT_TYPE = 'application/vnd.ponylang.libs-cache.v1'
CONFIG_TYPE = 'application/vnd.ponylang.libs-cache.config.v1+json'
MANIFEST_TYPE = 'application/vnd.oci.image.manifest.v1+json'
ARCHIVE_TYPE = 'application/vnd.ponylang.libs-cache.archive.v1.tar+gzip'

# A builder image is ghcr.io/ponylang/ponyc-ci-<name>:<YYYYMMDD>. The platform
# carries everything after the redundant `ponyc-ci-` prefix plus the date tag.
IMAGE_RE = re.compile(r'^ghcr\.io/ponylang/ponyc-ci-(.+):(\d{8})$')

# Paths (relative to the repo root) that make up the cached artifact.
CACHED_PATHS = [
    'build/libs',
    'lib/llvm/src/compiler-rt/lib/builtins',
]


def die(message):
    print(ERROR + message + ENDC, file=sys.stderr)
    sys.exit(1)


def info(message):
    print(INFO + message + ENDC)


def require_env(name):
    value = os.environ.get(name, '')
    if not value:
        die(f"{name} needs to be set in env. Exiting.")
    return value


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def owner():
    # GITHUB_REPOSITORY is "OWNER/REPO"; the registry namespace is the owner.
    repo = require_env('GITHUB_REPOSITORY')
    return repo.split('/')[0]


def repository(package):
    return f'{owner()}/{package}'


def derive_platform(image):
    match = IMAGE_RE.match(image)
    if not match:
        die(f"Builder image '{image}' does not match the expected format "
            "ghcr.io/ponylang/ponyc-ci-<name>:<YYYYMMDD>. If the builder image "
            "naming changed, update IMAGE_RE in branch_libs_cache.py.")
    name, date = match.group(1), match.group(2)
    # `:` -> `_` is injective here: image names use only `-` and `.`, never `_`.
    return f'{name}_{date}'


def cache_package(platform_id, pr):
    """Assemble the full GHCR package name for a platform/arch/PR.

    The one place namespace, architecture, and PR number are attached, so a pull
    and a push for the same PR on the same platform always agree on the name.
    """
    arch = platform.machine().lower()
    return f'{NAMESPACE}/{platform_id}-{arch}-pr{pr}'


def resolve_package(args):
    if args.image:
        platform_id = derive_platform(args.image)
    else:
        platform_id = args.platform
    return cache_package(platform_id, args.pr)


def digest_of(data):
    return 'sha256:' + hashlib.sha256(data).hexdigest()


def sha512_hex(data):
    return hashlib.sha512(data).hexdigest()


def request(method, url, headers=None, data=None):
    """Perform an HTTP request, returning (status, response_headers, body)."""
    req = urllib.request.Request(url, data=data, headers=headers or {},
                                 method=method)
    try:
        with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as r:
            return r.status, dict(r.headers), r.read()
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()
    except (urllib.error.URLError, OSError) as e:
        die(f"{method} {url} failed: {e}")


def check(status, body, method, url, ok):
    if status not in ok:
        text = body.decode('utf-8', errors='replace') if body else ''
        die(f"{method} {url} returned {status} (expected {ok})\n{text}")


def auth_token(package, scope):
    """Fetch a registry v2 token for the given scope.

    Uses GITHUB_TOKEN when present (required for push and for pulling private
    same-owner packages); falls back to an anonymous token otherwise.
    """
    repo = repository(package)
    headers = {}
    token = os.environ.get('GITHUB_TOKEN', '')
    if token:
        user = os.environ.get('GITHUB_ACTOR') or owner()
        creds = base64.b64encode(f"{user}:{token}".encode()).decode()
        headers['Authorization'] = f'Basic {creds}'
    query = urllib.parse.urlencode({
        'service': 'ghcr.io',
        'scope': f'repository:{repo}:{scope}',
    })
    url = f'{REGISTRY}/token?{query}'
    status, _, body = request('GET', url, headers=headers)
    check(status, body, 'GET', url, (200,))
    return json.loads(body)['token']


def blob_exists(repo, digest, token):
    url = f'{REGISTRY}/v2/{repo}/blobs/{digest}'
    status, _, body = request('HEAD', url,
                              headers={'Authorization': f'Bearer {token}'})
    if status == 200:
        return True
    if status == 404:
        return False
    check(status, body, 'HEAD', url, (200, 404))


def upload_blob(repo, data, digest, token):
    if blob_exists(repo, digest, token):
        info(f"Blob {digest} already present, skipping upload.")
        return
    # Start an upload session, then PUT the blob monolithically with the digest.
    start = f'{REGISTRY}/v2/{repo}/blobs/uploads/'
    status, resp_headers, body = request(
        'POST', start, headers={'Authorization': f'Bearer {token}'})
    check(status, body, 'POST', start, (202,))
    location = resp_headers.get('Location')
    if not location:
        die(f"POST {start} returned no Location header")
    if location.startswith('/'):
        location = REGISTRY + location
    sep = '&' if '?' in location else '?'
    put_url = f'{location}{sep}digest={digest}'
    status, _, body = request('PUT', put_url, data=data, headers={
        'Authorization': f'Bearer {token}',
        'Content-Type': 'application/octet-stream',
    })
    check(status, body, 'PUT', put_url, (201,))


def put_manifest(repo, tag, manifest, token):
    url = f'{REGISTRY}/v2/{repo}/manifests/{tag}'
    status, _, body = request('PUT', url, data=manifest, headers={
        'Authorization': f'Bearer {token}',
        'Content-Type': MANIFEST_TYPE,
    })
    check(status, body, 'PUT', url, (201,))


def get_manifest(repo, tag, token):
    """Return the parsed manifest, or None on a miss (404 / no access)."""
    url = f'{REGISTRY}/v2/{repo}/manifests/{tag}'
    status, _, body = request('GET', url, headers={
        'Authorization': f'Bearer {token}',
        'Accept': MANIFEST_TYPE,
    })
    if status in (401, 403, 404):
        return None
    check(status, body, 'GET', url, (200,))
    return json.loads(body)


def get_blob(repo, digest, token):
    url = f'{REGISTRY}/v2/{repo}/blobs/{digest}'
    status, _, body = request('GET', url,
                              headers={'Authorization': f'Bearer {token}'})
    check(status, body, 'GET', url, (200,))
    return body


def build_archive():
    """Tar+gzip the cached paths into in-memory bytes (arcnames repo-relative)."""
    root = repo_root()
    present = [p for p in CACHED_PATHS if os.path.exists(os.path.join(root, p))]
    if 'build/libs' not in present:
        die("build/libs does not exist; nothing to push. Build libs first.")
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode='w:gz') as tar:
        for path in present:
            tar.add(os.path.join(root, path), arcname=path)
    return buf.getvalue()


def extract_archive(data):
    root = repo_root()
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as tar:
        # Python 3.14 makes extractall default to the 'data' filter, which
        # would drop some members; 3.8-3.11 have no `filter` kwarg at all. Pin
        # to 'tar' (a faithful intra-tree restore that still rejects absolute
        # paths and `..` escapes) where the kwarg exists, and fall back to the
        # plain extract on older Pythons, so the restored tree is identical
        # across every runner's Python version.
        if sys.version_info >= (3, 12):
            tar.extractall(root, filter='tar')
        else:
            tar.extractall(root)


def cmd_package_name(package):
    print(package)


def cmd_exists(package, tag):
    """Report whether the artifact exists, without downloading the blob.

    Exits 0 when the manifest is present, 1 otherwise. A miss returns None from
    get_manifest; any other HTTP code or a network failure routes through
    check/request -> die (exit 1). So every non-present outcome exits non-zero,
    and a caller gating a build always fails safe to building.
    """
    repo = repository(package)
    info(f"Checking branch libs cache {REGISTRY}/{repo}:{tag}")
    token = auth_token(package, 'pull')
    if get_manifest(repo, tag, token) is None:
        info("Branch libs cache miss.")
        sys.exit(1)
    info("Branch libs cache present.")


def cmd_pull(package, tag):
    repo = repository(package)
    info(f"Pulling branch libs cache from {REGISTRY}/{repo}:{tag}")
    token = auth_token(package, 'pull')
    manifest = get_manifest(repo, tag, token)
    if manifest is None:
        info("Branch libs cache miss.")
        sys.exit(1)
    layers = manifest.get('layers') or []
    if not layers:
        die("Manifest has no layers; cannot restore branch libs cache.")
    digest = layers[0]['digest']
    info(f"Downloading blob {digest}...")
    data = get_blob(repo, digest, token)
    if digest_of(data) != digest:
        die("Downloaded blob digest mismatch; refusing to use it.")
    info("Extracting branch libs cache...")
    extract_archive(data)
    info("Branch libs cache restored.")


def cmd_push(package, tag):
    repo = repository(package)
    info(f"Pushing branch libs cache to {REGISTRY}/{repo}:{tag}")

    archive_data = build_archive()
    built_at = (datetime.datetime.now(datetime.timezone.utc)
                .strftime('%Y-%m-%dT%H:%M:%SZ'))
    config = json.dumps({
        'package': package,
        'tag': tag,
        'sha512': sha512_hex(archive_data),
        'built_at': built_at,
    }, sort_keys=True).encode('utf-8')

    archive_digest = digest_of(archive_data)
    config_digest = digest_of(config)

    token = auth_token(package, 'pull,push')

    info("Uploading config blob...")
    upload_blob(repo, config, config_digest, token)
    info("Uploading archive blob...")
    upload_blob(repo, archive_data, archive_digest, token)

    # Blob before manifest: a consumer pulling mid-push never sees a manifest
    # that references a blob that is not yet fully uploaded.
    manifest = json.dumps({
        'schemaVersion': 2,
        'mediaType': MANIFEST_TYPE,
        'artifactType': ARTIFACT_TYPE,
        'config': {
            'mediaType': CONFIG_TYPE,
            'digest': config_digest,
            'size': len(config),
        },
        'layers': [{
            'mediaType': ARCHIVE_TYPE,
            'digest': archive_digest,
            'size': len(archive_data),
            'annotations': {
                'org.opencontainers.image.title': 'libs.tar.gz',
            },
        }],
        'annotations': {
            'org.opencontainers.image.created': built_at,
        },
    }).encode('utf-8')

    info(f"Putting manifest {tag}...")
    put_manifest(repo, tag, manifest, token)
    info("Push complete.")


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('package-name', 'exists', 'pull', 'push'):
        p = sub.add_parser(name)
        target = p.add_mutually_exclusive_group(required=True)
        target.add_argument('--image', help='builder image reference')
        target.add_argument('--platform', help='literal platform label')
        p.add_argument('--pr', required=True, help='pull-request number')
        if name != 'package-name':
            p.add_argument('--tag', required=True,
                           help='hashFiles content hash')
    return parser


def main(argv):
    args = build_parser().parse_args(argv[1:])
    package = resolve_package(args)
    if args.command == 'package-name':
        cmd_package_name(package)
    elif args.command == 'exists':
        cmd_exists(package, args.tag)
    elif args.command == 'pull':
        cmd_pull(package, args.tag)
    elif args.command == 'push':
        cmd_push(package, args.tag)


if __name__ == '__main__':
    main(sys.argv)
