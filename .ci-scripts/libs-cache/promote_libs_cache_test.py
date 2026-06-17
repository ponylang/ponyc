#!/usr/bin/env python3
"""Self-contained tests for the promote path (no pytest; python3-runnable).

Run: python3 .ci-scripts/libs-cache/promote_libs_cache_test.py
Exits 0 if every check passes, 1 otherwise.

`registry.copy` is the registry-to-registry promote the warmer uses to reuse a
branch build instead of cold-building. Monkeypatching the registry helpers
(auth_token/get_manifest/get_blob/upload_blob/put_manifest) lets us assert the copy
sequence and -- the bug most likely to slip through -- that the rewritten manifest
declares the NEW config's size and digest, names the destination package, and
records the promotion provenance, all without any network. `promote_libs_cache.py`
is the thin entry that resolves the branch (src) and main (dst) names and calls
`registry.copy`; we pin that wiring too.
"""
import json
import os
import sys

import promote_libs_cache as promote
import registry

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


def _src_artifact(src_pkg, tag):
    """A realistic source manifest + its blobs, with real digests so copy()'s
    integrity check passes."""
    archive = b'fake-llvm-archive-bytes'
    archive_digest = registry.digest_of(archive)
    src_config = json.dumps({
        'package': src_pkg, 'tag': tag, 'sha512': registry.sha512_hex(archive),
        'built_at': '2026-01-02T03:04:05Z',
    }, sort_keys=True).encode('utf-8')
    config_digest = registry.digest_of(src_config)
    manifest = {
        'schemaVersion': 2,
        'mediaType': registry.MANIFEST_TYPE,
        'config': {'mediaType': registry.CONFIG_TYPE, 'digest': config_digest,
                   'size': len(src_config)},
        'layers': [{'mediaType': registry.ARCHIVE_TYPE, 'digest': archive_digest,
                    'size': len(archive),
                    'annotations': {'org.opencontainers.image.title':
                                    'libs.tar.gz'}}],
    }
    return archive, archive_digest, src_config, config_digest, manifest


def _patch_registry(manifest, blobs, calls, uploads, put):
    """Install fakes; return the saved originals tuple for restore."""
    def fake_auth(pkg, scope):
        calls.append(('auth', scope))
        return 'tok'

    def fake_get_manifest(repo, tag, token):
        calls.append(('get_manifest', repo))
        return manifest

    def fake_get_blob(repo, digest, token):
        calls.append(('get_blob', digest))
        if digest not in blobs:
            raise AssertionError(f"unexpected blob {digest}")
        return blobs[digest]

    def fake_upload_blob(repo, data, digest, token):
        calls.append(('upload_blob', repo))
        uploads[digest] = data

    def fake_put_manifest(repo, tag, manifest_bytes, token):
        calls.append(('put_manifest', repo))
        put['repo'] = repo
        put['manifest'] = json.loads(manifest_bytes)

    saved = (registry.auth_token, registry.get_manifest, registry.get_blob,
             registry.upload_blob, registry.put_manifest)
    registry.auth_token = fake_auth
    registry.get_manifest = fake_get_manifest
    registry.get_blob = fake_get_blob
    registry.upload_blob = fake_upload_blob
    registry.put_manifest = fake_put_manifest
    return saved


def _restore(saved):
    (registry.auth_token, registry.get_manifest, registry.get_blob,
     registry.upload_blob, registry.put_manifest) = saved


def test_copy_promotes_with_fresh_consistent_manifest():
    os.environ['GITHUB_REPOSITORY'] = 'ponylang/ponyc'
    src_pkg = 'ponyc-branch-libs-cache/plat-x86_64'
    dst_pkg = 'ponyc-libs-cache/plat-x86_64'
    tag = 'deadbeef'
    archive, archive_digest, src_config, config_digest, manifest = \
        _src_artifact(src_pkg, tag)
    blobs = {archive_digest: archive, config_digest: src_config}
    calls, uploads, put = [], {}, {}

    saved = _patch_registry(manifest, blobs, calls, uploads, put)
    try:
        registry.copy(src_pkg, dst_pkg, tag)
    finally:
        _restore(saved)

    # Sequence: pull-scope token, manifest, archive blob, config blob (for
    # built_at), push-scope token, two uploads, manifest -- blob before manifest.
    check('verbs', [c[0] for c in calls],
          ['auth', 'get_manifest', 'get_blob', 'get_blob', 'auth',
           'upload_blob', 'upload_blob', 'put_manifest'])
    check('src pull scope', calls[0], ('auth', 'pull'))
    check('dst push scope', calls[4], ('auth', 'pull,push'))
    check('archive downloaded first', calls[2], ('get_blob', archive_digest))

    new_manifest = put['manifest']
    new_config_digest = new_manifest['config']['digest']
    new_config = json.loads(uploads[new_config_digest])
    check('config names dst', new_config['package'], dst_pkg)
    check('config promoted_from', new_config['promoted_from'], src_pkg)
    check('config carries built_at', new_config['built_at'],
          '2026-01-02T03:04:05Z')
    check('config has promoted_at', 'promoted_at' in new_config, True)
    # The bug the plan's review flagged: config.size must be the NEW config's
    # length, not the source's.
    check('manifest config.size recomputed',
          new_manifest['config']['size'], len(uploads[new_config_digest]))
    check('manifest config.digest matches uploaded',
          new_config_digest in uploads, True)
    # Layer block reused verbatim (digest + size) and the archive re-uploaded.
    check('layer digest reused', new_manifest['layers'][0]['digest'],
          archive_digest)
    check('layer size reused', new_manifest['layers'][0]['size'], len(archive))
    check('archive uploaded to dst', archive_digest in uploads, True)


def test_copy_aborts_on_archive_digest_mismatch():
    os.environ['GITHUB_REPOSITORY'] = 'ponylang/ponyc'
    src_pkg = 'ponyc-branch-libs-cache/plat-x86_64'
    dst_pkg = 'ponyc-libs-cache/plat-x86_64'
    tag = 'deadbeef'
    _, archive_digest, _, config_digest, manifest = _src_artifact(src_pkg, tag)
    # The registry hands back corrupted bytes for the archive digest.
    blobs = {archive_digest: b'corrupted-does-not-hash-to-digest'}
    calls, uploads, put = [], {}, {}

    saved = _patch_registry(manifest, blobs, calls, uploads, put)
    try:
        registry.copy(src_pkg, dst_pkg, tag)
        FAILURES.append('digest mismatch did not abort')
    except SystemExit as e:
        check('mismatch exit code', e.code, 1)
        check('mismatch wrote nothing', uploads, {})
        check('mismatch put no manifest', put, {})
    finally:
        _restore(saved)


def test_entry_resolves_branch_src_and_main_dst():
    # promote_libs_cache.main resolves the BRANCH name as source and the MAIN name
    # as destination, and passes the tag through.
    captured = {}

    def fake_copy(src, dst, tag):
        captured['src'] = src
        captured['dst'] = dst
        captured['tag'] = tag

    saved = registry.copy
    registry.copy = fake_copy
    try:
        promote.main(['promote_libs_cache.py', '--platform', 'mylabel',
                      '--tag', 'cafef00d'])
    finally:
        registry.copy = saved

    check('src is branch namespace',
          captured['src'].startswith('ponyc-branch-libs-cache/mylabel-'), True)
    check('dst is main namespace',
          captured['dst'].startswith('ponyc-libs-cache/mylabel-'), True)
    check('tag passed through', captured['tag'], 'cafef00d')
    # Same suffix under two prefixes (the promotion invariant).
    check('same suffix',
          captured['src'][len('ponyc-branch-libs-cache/'):],
          captured['dst'][len('ponyc-libs-cache/'):])


TESTS = [test_copy_promotes_with_fresh_consistent_manifest,
         test_copy_aborts_on_archive_digest_mismatch,
         test_entry_resolves_branch_src_and_main_dst]


def main():
    for t in TESTS:
        t()
    if FAILURES:
        print(f"FAIL ({len(FAILURES)}):")
        for f in FAILURES:
            print(f"  {f}")
        sys.exit(1)
    print(f"ok ({len(TESTS)} test functions)")


if __name__ == '__main__':
    main()
