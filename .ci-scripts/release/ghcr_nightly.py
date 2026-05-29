#!/usr/bin/env python3
"""Publish a nightly tool archive to GHCR as an OCI artifact.

Stdlib only so no pip install is required on any CI runner, matching the
posture of github_release.py.

Each (tool, platform) pair is one OCI image under a `nightly/` path segment:

    ghcr.io/ponylang/nightly/<tool>-<triple>:<YYYYMMDD>

See https://github.com/ponylang/ponyup/discussions/412 for the design.

Usage:
    ghcr_nightly.py push <tool> <triple> <version> <archive>

`push` uploads the archive, uploads a config blob carrying metadata, and PUTs
the OCI manifest. Auth uses GITHUB_TOKEN (needs `packages: write`).
"""

import base64
import datetime
import hashlib
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

ENDC = '\033[0m'
ERROR = '\033[31m'
INFO = '\033[34m'

REGISTRY = 'https://ghcr.io'
NAMESPACE = 'nightly'
REQUEST_TIMEOUT = 300

ARTIFACT_TYPE = 'application/vnd.ponylang.nightly.v1'
CONFIG_TYPE = 'application/vnd.ponylang.nightly.config.v1+json'
MANIFEST_TYPE = 'application/vnd.oci.image.manifest.v1+json'
ARCHIVE_TYPE_TARGZ = 'application/vnd.ponylang.nightly.archive.v1.tar+gzip'
ARCHIVE_TYPE_ZIP = 'application/vnd.ponylang.nightly.archive.v1.zip'


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


def owner():
    # GITHUB_REPOSITORY is "OWNER/REPO"; the registry namespace is the owner.
    repo = require_env('GITHUB_REPOSITORY')
    return repo.split('/')[0]


def package_name(tool, triple):
    return f'{NAMESPACE}/{tool}-{triple}'


def repository(tool, triple):
    return f'{owner()}/{package_name(tool, triple)}'


def archive_media_type(archive):
    if archive.endswith('.zip'):
        return ARCHIVE_TYPE_ZIP
    if archive.endswith('.tar.gz'):
        return ARCHIVE_TYPE_TARGZ
    die(f"Unsupported archive extension: {archive}")


def digest_of(data):
    return 'sha256:' + hashlib.sha256(data).hexdigest()


def sha512_hex(data):
    return hashlib.sha512(data).hexdigest()


def request(method, url, headers=None, data=None):
    """Perform an HTTP request, returning (status, response_headers, body).

    Non-2xx responses are returned rather than raised so callers can decide;
    network-level failures are fatal.
    """
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


def pull_push_token(repo):
    # Docker registry v2 token exchange. GHCR authenticates the token; the
    # Basic-auth username is not significant, so fall back to the owner when
    # GITHUB_ACTOR is unset (e.g. a bare `docker run`).
    token = require_env('GITHUB_TOKEN')
    user = os.environ.get('GITHUB_ACTOR') or owner()
    creds = base64.b64encode(f"{user}:{token}".encode()).decode()
    scope = f'repository:{repo}:pull,push'
    query = urllib.parse.urlencode({'service': 'ghcr.io', 'scope': scope})
    url = f'{REGISTRY}/token?{query}'
    status, _, body = request('GET', url,
                              headers={'Authorization': f'Basic {creds}'})
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


def cmd_push(tool, triple, version, archive):
    if not os.path.isfile(archive):
        die(f"File not found: {archive}")

    repo = repository(tool, triple)
    info(f"Publishing {os.path.basename(archive)} to "
         f"{REGISTRY}/{repo}:{version}")

    with open(archive, 'rb') as f:
        archive_data = f.read()
    built_at = (datetime.datetime.now(datetime.timezone.utc)
                .strftime('%Y-%m-%dT%H:%M:%SZ'))
    config = json.dumps({
        'tool': tool,
        'platform': triple,
        'version': version,
        'sha512': sha512_hex(archive_data),
        'built_at': built_at,
    }, sort_keys=True).encode('utf-8')

    archive_digest = digest_of(archive_data)
    config_digest = digest_of(config)

    token = pull_push_token(repo)

    info("Uploading config blob...")
    upload_blob(repo, config, config_digest, token)
    info("Uploading archive blob...")
    upload_blob(repo, archive_data, archive_digest, token)

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
            'mediaType': archive_media_type(archive),
            'digest': archive_digest,
            'size': len(archive_data),
            'annotations': {
                'org.opencontainers.image.title': os.path.basename(archive),
            },
        }],
        'annotations': {
            'org.opencontainers.image.created': built_at,
        },
    }).encode('utf-8')

    info(f"Putting manifest {version}...")
    put_manifest(repo, version, manifest, token)
    info("Publish complete.")


def usage():
    die("usage: ghcr_nightly.py push <tool> <triple> <version> <archive>")


def main(argv):
    if len(argv) < 2:
        usage()
    command = argv[1]
    if command == 'push':
        if len(argv) != 6:
            usage()
        cmd_push(argv[2], argv[3], argv[4], argv[5])
    else:
        usage()


if __name__ == '__main__':
    main(sys.argv)
