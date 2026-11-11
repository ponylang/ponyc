#!/usr/bin/env python3
"""Self-contained tests of cache-script exit codes against a fake registry.

Run: python3 .ci-scripts/libs-cache/registry_exit_codes_test.py
Exits 0 if every check passes, 1 otherwise.

Callers read 0 as present, EXIT_MISS as a definite cache miss, and any other
code as an error. Only a manifest 404 is a miss; a bad response, a malformed
manifest or archive, or a crash anywhere in the script must not exit EXIT_MISS.
This test runs the real entry scripts as subprocesses, the way CI does, against
a local `http.server` registry.

The scripts are copied into a temporary tree at `<tmp>/.ci-scripts/libs-cache/`
with `registry.REGISTRY` rewritten to the fake. `registry.repo_root()` then
resolves to `<tmp>`, so a `pull` extracts there and never into this checkout.
"""
import glob
import hashlib
import http.server
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading

from common import EXIT_MISS

HERE = os.path.dirname(os.path.abspath(__file__))
RUN_TIMEOUT = 60

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def digest(data):
    return 'sha256:' + hashlib.sha256(data).hexdigest()


def tar_gz(files):
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode='w:gz') as tar:
        for name, data in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
    return buf.getvalue()


GOOD_ARCHIVE = tar_gz({'build/libs/restored.txt': b'libs\n'})
ARCHIVES = {
    'ok': GOOD_ARCHIVE,
    'not-gzip': b'this is not a gzip stream',
    'truncated-gzip': GOOD_ARCHIVE[:len(GOOD_ARCHIVE) // 2],
}
CONFIG = json.dumps({'built_at': '2026-01-01T00:00:00Z'}).encode()


class Registry(http.server.BaseHTTPRequestHandler):
    """A registry whose responses come from `self.server.scenario`.

    The scenario keys are 'token', 'manifest', and 'blob'. The handler keeps
    the default HTTP/1.0, so the connection closes after each response and a
    body shorter than its Content-Length raises IncompleteRead in the client.
    """

    def log_message(self, *args):
        pass

    def reply(self, status, body=b'', length=None):
        self.send_response(status)
        self.send_header('Content-Length',
                         str(len(body) if length is None else length))
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(body)

    def short(self, body):
        self.reply(200, body, length=len(body) + 100)

    def do_GET(self):
        scenario = self.server.scenario
        if self.path.startswith('/token'):
            self.token(scenario['token'])
        elif '/manifests/' in self.path:
            self.manifest(scenario['manifest'], scenario['blob'])
        elif '/blobs/' in self.path:
            self.blob(scenario['blob'])
        else:
            self.reply(404)

    def token(self, kind):
        if kind == 'ok':
            self.reply(200, json.dumps({'token': 't'}).encode())
        elif kind == 'html':
            self.reply(200, b'<html>not json</html>')
        elif kind == 'no-token-key':
            self.reply(200, b'{}')
        elif kind == 'short':
            self.short(json.dumps({'token': 't'}).encode())
        elif kind == 'bad-status-line':
            self.wfile.write(b'garbage\r\n\r\n')

    def manifest(self, kind, blob_kind):
        archive = ARCHIVES.get(blob_kind, GOOD_ARCHIVE)
        good = json.dumps({
            'schemaVersion': 2,
            'config': {'digest': digest(CONFIG), 'size': len(CONFIG)},
            'layers': [{'digest': digest(archive), 'size': len(archive)}],
        }).encode()
        if kind == 'ok':
            self.reply(200, good)
        elif kind == '404':
            self.reply(404)
        elif kind == '500':
            self.reply(500, b'internal error')
        elif kind == 'html':
            self.reply(200, b'<html>not json</html>')
        elif kind == 'array':
            self.reply(200, b'[]')
        elif kind == 'layer-without-digest':
            self.reply(200, json.dumps({
                'config': {'digest': digest(CONFIG)},
                'layers': [{'size': 1}],
            }).encode())
        elif kind == 'short':
            self.short(good)

    def blob(self, kind):
        if self.path.endswith(digest(CONFIG)):
            self.reply(200, CONFIG)
        elif kind == 'short':
            self.short(GOOD_ARCHIVE)
        else:
            self.reply(200, ARCHIVES[kind])

    # Promote uploads to the destination package.
    def do_HEAD(self):
        self.reply(404)

    def do_POST(self):
        self.send_response(202)
        self.send_header('Location', '/v2/upload/session')
        self.send_header('Content-Length', '0')
        self.end_headers()

    def do_PUT(self):
        self.rfile.read(int(self.headers.get('Content-Length', 0)))
        self.reply(201)


def make_tree(registry_url, drop=()):
    """Copy the non-test scripts into a temp tree pointed at the fake registry.
    `drop` names scripts to leave out. Returns (tmp, scripts_dir)."""
    tmp = tempfile.mkdtemp()
    scripts = os.path.join(tmp, '.ci-scripts', 'libs-cache')
    os.makedirs(scripts)
    for src in glob.glob(os.path.join(HERE, '*.py')):
        name = os.path.basename(src)
        if not name.endswith('_test.py') and name not in drop:
            shutil.copy(src, scripts)
    path = os.path.join(scripts, 'registry.py')
    with open(path) as f:
        text = f.read()
    old = "REGISTRY = 'https://ghcr.io'\n"
    if text.count(old) != 1:
        # Without the rewrite the scripts would run against the real registry.
        shutil.rmtree(tmp)
        raise RuntimeError(f"expected one {old.strip()!r} in registry.py")
    with open(path, 'w') as f:
        f.write(text.replace(old, f"REGISTRY = '{registry_url}'\n"))
    return tmp, scripts


def run(tmp, scripts, argv):
    """Run an entry script the way CI does; return its exit code."""
    env = {k: v for k, v in os.environ.items()
           if k.lower() not in ('http_proxy', 'https_proxy', 'all_proxy')}
    env.update({
        'GITHUB_REPOSITORY': 'owner/repo',
        'GITHUB_TOKEN': 'token',
        'GITHUB_ACTOR': 'actor',
        'GITHUB_WORKSPACE': tmp,
        'PYTHONDONTWRITEBYTECODE': '1',
    })
    try:
        proc = subprocess.run(
            [sys.executable, os.path.join(scripts, argv[0])] + argv[1:],
            cwd=tmp, env=env, capture_output=True, timeout=RUN_TIMEOUT)
    except subprocess.TimeoutExpired:
        return 'timeout'
    return proc.returncode


SELECTOR = ['--platform', 'p', '--tag', 't']
COMMANDS = {
    'exists': ['oci_libs_cache.py', 'exists'] + SELECTOR,
    'pull': ['oci_libs_cache.py', 'pull'] + SELECTOR,
    'branch exists': ['branch_libs_cache.py', 'exists'] + SELECTOR,
    'promote': ['promote_libs_cache.py'] + SELECTOR,
}

HIT, MISS, ERROR, TIMEOUT = 'hit', 'miss', 'error', 'timeout'


def outcome(code):
    if code == 'timeout':
        return TIMEOUT
    if code == 0:
        return HIT
    if code == EXIT_MISS:
        return MISS
    return ERROR


def scenario(token='ok', manifest='ok', blob='ok'):
    return {'token': token, 'manifest': manifest, 'blob': blob}


ALL_ERROR = {name: ERROR for name in COMMANDS}

# (label, scenario, expected outcome per command). `exists` parses the manifest
# but doesn't read its fields or download the archive, so a manifest that is
# valid JSON but the wrong shape, or a bad archive, counts as present. Promote
# copies the archive without extracting it, so a bad archive whose digest
# matches is copied successfully.
CASES = [
    ('all present', scenario(),
     {'exists': HIT, 'pull': HIT, 'branch exists': HIT, 'promote': HIT}),
    ('manifest 404', scenario(manifest='404'),
     {'exists': MISS, 'pull': MISS, 'branch exists': MISS, 'promote': ERROR}),
    ('manifest 500', scenario(manifest='500'), ALL_ERROR),
    ('token not JSON', scenario(token='html'), ALL_ERROR),
    ('token without token key', scenario(token='no-token-key'), ALL_ERROR),
    ('token body short', scenario(token='short'), ALL_ERROR),
    ('garbage status line', scenario(token='bad-status-line'), ALL_ERROR),
    ('manifest not JSON', scenario(manifest='html'), ALL_ERROR),
    ('manifest body short', scenario(manifest='short'), ALL_ERROR),
    ('manifest is an array', scenario(manifest='array'),
     {'exists': HIT, 'pull': ERROR, 'branch exists': HIT, 'promote': ERROR}),
    ('layer without digest', scenario(manifest='layer-without-digest'),
     {'exists': HIT, 'pull': ERROR, 'branch exists': HIT, 'promote': ERROR}),
    ('blob body short', scenario(blob='short'),
     {'exists': HIT, 'pull': ERROR, 'branch exists': HIT, 'promote': ERROR}),
    ('archive not gzip', scenario(blob='not-gzip'),
     {'exists': HIT, 'pull': ERROR, 'branch exists': HIT, 'promote': HIT}),
    ('archive gzip truncated', scenario(blob='truncated-gzip'),
     {'exists': HIT, 'pull': ERROR, 'branch exists': HIT, 'promote': HIT}),
]


def test_primitives(server, url):
    tmp, scripts = make_tree(url)
    try:
        for label, sc, want in CASES:
            server.scenario = sc
            for name, argv in COMMANDS.items():
                shutil.rmtree(os.path.join(tmp, 'build'), ignore_errors=True)
                code = run(tmp, scripts, argv)
                check(f"{label}, {name} (exit {code})", outcome(code),
                      want[name])
        server.scenario = scenario()
        run(tmp, scripts, COMMANDS['pull'])
        check('pull restores into the temp tree', os.path.exists(
            os.path.join(tmp, 'build', 'libs', 'restored.txt')), True)
    finally:
        shutil.rmtree(tmp)


def test_import_failure_is_not_a_miss(server, url):
    tmp, scripts = make_tree(url, drop=('cache_arch.py',))
    try:
        server.scenario = scenario(manifest='404')
        for name, argv in COMMANDS.items():
            code = run(tmp, scripts, argv)
            check(f"missing module, {name} (exit {code})", outcome(code), ERROR)
    finally:
        shutil.rmtree(tmp)


def test_usage_error_is_not_a_miss(server, url):
    tmp, scripts = make_tree(url)
    try:
        server.scenario = scenario(manifest='404')
        for name, argv in COMMANDS.items():
            code = run(tmp, scripts, argv + ['--no-such-flag'])
            check(f"usage error, {name} (exit {code})", outcome(code), ERROR)
    finally:
        shutil.rmtree(tmp)


SKIP_ON_MISS = ['resolve_libs_cache.py', '--require-cache-hit',
                '--skip-on-miss'] + SELECTOR


def test_skip_on_miss_skips_only_a_miss(server, url):
    tmp, scripts = make_tree(url)
    marker = os.path.join(tmp, '.libs-cache-miss')
    try:
        server.scenario = scenario(manifest='404')
        code = run(tmp, scripts, SKIP_ON_MISS)
        check('skip-on-miss, manifest 404 exit', code, 0)
        check('skip-on-miss, manifest 404 marker', os.path.exists(marker), True)
        if os.path.exists(marker):
            os.remove(marker)
        server.scenario = scenario(token='html')
        code = run(tmp, scripts, SKIP_ON_MISS)
        check(f"skip-on-miss, token not JSON fails (exit {code})", code != 0,
              True)
        check('skip-on-miss, token not JSON marker', os.path.exists(marker),
              False)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


TESTS = [test_primitives, test_import_failure_is_not_a_miss,
         test_usage_error_is_not_a_miss, test_skip_on_miss_skips_only_a_miss]


def main():
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Registry)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f'http://127.0.0.1:{server.server_port}'
    try:
        for t in TESTS:
            t(server, url)
    finally:
        server.shutdown()
        server.server_close()
    if FAILURES:
        print(f"FAIL ({len(FAILURES)}):")
        for f in FAILURES:
            print(f"  {f}")
        sys.exit(1)
    print(f"ok ({len(TESTS)} test functions)")


if __name__ == '__main__':
    main()
