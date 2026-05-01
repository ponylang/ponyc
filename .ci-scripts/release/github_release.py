#!/usr/bin/env python3
"""Upload ponyc release archives to a GitHub Release.

Stdlib only so no pip install is required on any CI runner.
"""

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

API_VERSION = '2022-11-28'
API_BASE = 'https://api.github.com'
UPLOAD_BASE = 'https://uploads.github.com'
REQUEST_TIMEOUT = 300


def die(message):
    print(ERROR + message + ENDC, file=sys.stderr)
    sys.exit(1)


def require_env(name):
    value = os.environ.get(name, '')
    if not value:
        die(f"{name} needs to be set in env. Exiting.")
    return value


def auth_headers(token):
    return {
        'Authorization': f'Bearer {token}',
        'Accept': 'application/vnd.github+json',
        'X-GitHub-Api-Version': API_VERSION,
    }


def api_request(method, url, token, data=None, content_type=None):
    headers = auth_headers(token)
    if content_type is not None:
        headers['Content-Type'] = content_type
    request = urllib.request.Request(url, data=data, headers=headers,
                                     method=method)
    try:
        with urllib.request.urlopen(request, timeout=REQUEST_TIMEOUT) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8', errors='replace')
        die(f"GitHub API {method} {url} failed: {e.code}\n{body}")
    except (urllib.error.URLError, OSError) as e:
        die(f"GitHub API {method} {url} failed: {e}")


def get_release(repo, tag, token):
    tag_q = urllib.parse.quote(tag, safe='')
    url = f'{API_BASE}/repos/{repo}/releases/tags/{tag_q}'
    _, body = api_request('GET', url, token)
    return json.loads(body)


def delete_asset(repo, asset_id, token):
    url = f'{API_BASE}/repos/{repo}/releases/assets/{asset_id}'
    api_request('DELETE', url, token)


def upload_asset(repo, release_id, path, token):
    name = os.path.basename(path)
    query = urllib.parse.urlencode({'name': name})
    url = (f'{UPLOAD_BASE}/repos/{repo}/releases/{release_id}/assets'
           f'?{query}')
    with open(path, 'rb') as f:
        data = f.read()
    api_request('POST', url, token, data=data,
                content_type='application/octet-stream')


def write_sha512_sibling(archive_path):
    # Raw hex + newline. NOT sha512sum CLI format (which appends "  <filename>");
    # consumers read the whole file as the hex digest. See migration design in
    # https://github.com/ponylang/ponyup/discussions/405.
    h = hashlib.sha512()
    with open(archive_path, 'rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    sibling_path = archive_path + '.sha512'
    # Binary mode: keep the digest file byte-identical across platforms.
    # Text mode on Windows would rewrite '\n' as '\r\n' and produce Windows
    # archives with CRLF-terminated siblings while Linux/macOS produce LF.
    with open(sibling_path, 'wb') as f:
        f.write((h.hexdigest() + '\n').encode('ascii'))
    return sibling_path


def cmd_upload(tag, path):
    token = require_env('RELEASE_TOKEN')
    repo = require_env('GITHUB_REPOSITORY')

    if not os.path.isfile(path):
        die(f"File not found: {path}")

    print(INFO + f"Writing SHA-512 sibling for {os.path.basename(path)}..."
          + ENDC)
    sibling_path = write_sha512_sibling(path)
    upload_paths = [path, sibling_path]
    upload_names = {os.path.basename(p) for p in upload_paths}

    print(INFO + f"Fetching release {tag}..." + ENDC)
    release = get_release(repo, tag, token)
    release_id = release['id']

    # Clobber any prior upload of the archive or its sibling so restarts
    # converge. A DELETE that succeeds followed by a POST that fails leaves
    # the release missing the asset; re-pushing the X.Y.Z tag re-runs this
    # script and reconverges. Delete every matching-name asset rather than
    # stopping at the first — the Releases API does not guarantee name
    # uniqueness across assets.
    for asset in release.get('assets', []):
        if asset.get('name') in upload_names:
            print(INFO + f"Deleting existing asset {asset['name']}..." + ENDC)
            delete_asset(repo, asset['id'], token)

    for upload_path in upload_paths:
        name = os.path.basename(upload_path)
        print(INFO + f"Uploading {name} to release {tag}..." + ENDC)
        upload_asset(repo, release_id, upload_path, token)
    print(INFO + "Upload complete." + ENDC)


def usage():
    die("usage: github_release.py upload <tag> <file>")


def main(argv):
    if len(argv) < 2:
        usage()
    command = argv[1]
    if command == 'upload':
        if len(argv) != 4:
            usage()
        cmd_upload(argv[2], argv[3])
    else:
        usage()


if __name__ == '__main__':
    main(sys.argv)
