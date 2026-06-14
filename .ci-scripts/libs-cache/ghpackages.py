#!/usr/bin/env python3
"""GitHub Packages REST client for the libs-cache retention/clear scripts.

The cache's retention (`prune_libs_cache.py`, `prune_branch_libs_cache.py`) and
clear (`clear_libs_cache.py`) scripts enumerate and delete GHCR packages via the
GitHub REST API -- a different surface from the registry-v2 API in `registry.py`
that the push/pull scripts use. This module is the shared REST plumbing:
`gh_request` (optionally with a JSON `data` body, for the warmer dispatch),
paginated `paginate`, and `encode` for package names (which contain `/` and must
be percent-encoded in the path). The two-token split (org PAT to enumerate,
GITHUB_TOKEN to delete) lives in each caller -- this module just takes the token.

Stdlib only so no pip install is required on any CI runner.
"""

import json
import urllib.error
import urllib.parse
import urllib.request

from common import die

API = 'https://api.github.com'
API_VERSION = '2026-03-10'
REQUEST_TIMEOUT = 120


def gh_request(method, path, token, data=None):
    """Call the GitHub REST API, returning (status, response_headers, body)."""
    url = path if path.startswith('http') else f'{API}{path}'
    headers = {
        'Authorization': f'Bearer {token}',
        'Accept': 'application/vnd.github+json',
        'X-GitHub-Api-Version': API_VERSION,
    }
    body = json.dumps(data).encode('utf-8') if data is not None else None
    if body is not None:
        headers['Content-Type'] = 'application/json'
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as r:
            return r.status, dict(r.headers), r.read()
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()
    except (urllib.error.URLError, OSError) as e:
        die(f"{method} {url} failed: {e}")


def paginate(path, token):
    """Yield items across all pages of a list endpoint."""
    page = 1
    while True:
        sep = '&' if '?' in path else '?'
        url = f'{path}{sep}per_page=100&page={page}'
        status, _, body = gh_request('GET', url, token)
        if status != 200:
            text = body.decode('utf-8', errors='replace') if body else ''
            die(f"GET {url} returned {status}\n{text}")
        items = json.loads(body)
        if not items:
            return
        yield from items
        if len(items) < 100:
            return
        page += 1


def encode(name):
    # A package name contains `/` (the namespace separator); it must be
    # percent-encoded in the REST API path.
    return urllib.parse.quote(name, safe='')
