#!/usr/bin/env python3
"""Self-contained tests for pr_changed_suites.py (no pytest; python3-runnable).

Run: python3 .ci-scripts/pr_changed_suites_test.py
Exits 0 if every check passes, 1 otherwise.
"""
import io
import sys

import pr_changed_suites as c

FAILURES = []


def check(label, got, want):
    if got != want:
        FAILURES.append(f"{label}: got {got!r}, want {want!r}")


# (path, expected (ponyc, pony_compiler, tools, net_ssl)) -- one row per
# distinguishing case. Each suite predicate is exercised at every boundary it
# draws.
SINGLE_PATH_TABLE = [
    # runtime code: ponyc + tools, but not the compiler suite or net_ssl.
    ('src/libponyrt/foo.c', (True, False, True, False)),
    # libponyc source triggers ponyc, pony_compiler, and tools.
    ('src/libponyc/foo.c', (True, True, True, False)),
    # THE GOTCHA: a libponyc doc/yaml is excluded from ponyc and tools (doc/yaml
    # filtered) but still triggers pony_compiler (its filter has no exclusions).
    ('src/libponyc/README.md', (False, True, False, False)),
    ('src/libponyc/notes.yml', (False, True, False, False)),
    # tools code that is NOT the pony_compiler library: tools only.
    ('tools/pony-lint/config.pony', (False, False, True, False)),
    # the pony_compiler library lives under tools/, so ponyc (no tools/) skips
    # it while pony_compiler and tools both run.
    ('tools/lib/ponylang/pony_compiler/pass.pony', (False, True, True, False)),
    # plain stdlib code (not under packages/net/): ponyc + tools.
    ('packages/json/json.pony', (True, False, True, False)),
    # packages/net/ code triggers net_ssl as well as ponyc + tools.
    ('packages/net/ssl.pony', (True, False, True, True)),
    ('packages/net/_ssl.pony', (True, False, True, True)),
    ('packages/net/tcp_listener.pony', (True, False, True, True)),
    # a doc under packages/net/ is excluded from everything.
    ('packages/net/README.md', (False, False, False, False)),
    # the shared CMake build system builds pony-compiler-tests, so it triggers
    # pony_compiler too (as well as ponyc and tools via their broad rules).
    ('cmake/PonyBinary.cmake', (True, True, True, False)),
    ('CMakeLists.txt', (True, True, True, False)),
    ('CMakePresets.json', (True, True, True, False)),
    # boundary guards for the build-system rule. Exact-match, not endswith: a
    # tool's own CMakeLists and the vendored-LLVM (lib/) presets are NOT the
    # top-level files. Prefix needs the slash: a `cmakefoo/` sibling must not
    # match `cmake/`. And a build-system doc stays excluded everywhere, keeping
    # the classifier in sync with the union filter (which drops **/*.md).
    ('tools/pony-lint/CMakeLists.txt', (False, False, True, False)),
    ('lib/CMakePresets.json', (True, False, True, False)),
    ('cmakefoo/build.cmake', (True, False, True, False)),
    ('cmake/notes.md', (False, False, False, False)),
    # generative, tcp-swarm, and udp-swarm .pony source triggers the ponyc
    # suite (not excluded like the rest of test/rt-stress/). Python and
    # rt-systematic stay excluded.
    ('test/rt-stress/generative/main.pony', (True, False, True, False)),
    ('test/rt-stress/generative/orchestrate_normal_test.py', (False, False,
                                                              False, False)),
    ('test/rt-stress/generative/stress_common.py', (False, False,
                                                    False, False)),
    ('test/rt-stress/tcp-swarm/tcp_swarm.pony', (True, False, True, False)),
    ('test/rt-stress/tcp-swarm/orchestrate_tcp.py', (False, False,
                                                     False, False)),
    ('test/rt-stress/udp-swarm/udp_flood.pony', (True, False, True, False)),
    ('test/rt-stress/udp-swarm/udp_swarm.pony', (True, False, True, False)),
    ('test/rt-stress/udp-swarm/orchestrate_udp.py', (False, False,
                                                     False, False)),
    ('test/rt-stress/suspend-drain/suspend_drain.pony', (False, False,
                                                         False, False)),
    ('test/rt-systematic/order-signature/main.pony', (False, False,
                                                      False, False)),
    # ...but the still-built test/ subdirs (compiled by test-ci-core) stay IN,
    # and a sibling like test/rt-stress-foo/ stays IN -- guard rows so an
    # over-broad exclusion (widened prefix, dropped trailing slash) can't
    # silently drop a suite with the table still green.
    ('test/libponyc/array.cc', (True, False, True, False)),
    ('test/rt-stress-foo/bar.pony', (True, False, True, False)),
    # excluded files trigger nothing (and are not pony_compiler paths).
    ('README.md', (False, False, False, False)),
    ('config.yaml.yml', (False, False, False, False)),
    ('.dockerfiles/x86-64-unknown-linux/Dockerfile', (False, False,
                                                      False, False)),
    ('.ci-dockerfiles/build.sh', (False, False, False, False)),
    ('.gitignore', (False, False, False, False)),
    ('.markdownlintignore', (False, False, False, False)),
    # the merged workflow file re-includes into every suite.
    ('.github/workflows/pr.yml', (True, True, True, True)),
    # any OTHER workflow yaml is excluded everywhere (yaml filter, not the WF).
    ('.github/workflows/release.yml', (False, False, False, False)),
]


def test_single_path_table():
    for path, want in SINGLE_PATH_TABLE:
        result = c.classify([path])
        got = (result['ponyc'], result['pony_compiler'], result['tools'],
               result['net_ssl'])
        check(f"classify([{path!r}])", got, want)


def test_or_aggregation():
    # A suite is on if ANY changed file triggers it.
    result = c.classify(['README.md', 'src/libponyc/foo.c'])
    check("aggregate ponyc", result['ponyc'], True)
    check("aggregate pony_compiler", result['pony_compiler'], True)
    check("aggregate tools", result['tools'], True)
    check("aggregate net_ssl", result['net_ssl'], False)
    # net_ssl lights up when a packages/net/ file is in the mix.
    result2 = c.classify(['README.md', 'packages/net/ssl.pony'])
    check("aggregate net_ssl on", result2['net_ssl'], True)
    # Doc-only change triggers nothing.
    docs = c.classify(['README.md', 'docs/guide.md'])
    check("docs-only", docs, {'ponyc': False, 'pony_compiler': False,
                              'tools': False, 'net_ssl': False})


def test_empty_changeset():
    check("empty", c.classify([]),
          {'ponyc': False, 'pony_compiler': False, 'tools': False,
           'net_ssl': False})


def test_ponyc_subset_of_tools():
    # ponyc must be a subset of tools, so the workflow-level union (tools plus
    # pony_compiler) never skips a ponyc-triggering file.
    for path, _ in SINGLE_PATH_TABLE:
        r = c.classify([path])
        if r['ponyc']:
            check(f"{path!r} ponyc=>tools", r['tools'], True)


def test_net_ssl_subset_of_ponyc():
    # net_ssl must be a subset of ponyc, so the union (tools plus
    # pony_compiler) covers it transitively.
    for path, _ in SINGLE_PATH_TABLE:
        r = c.classify([path])
        if r['net_ssl']:
            check(f"{path!r} net_ssl=>ponyc", r['ponyc'], True)


def test_render_format():
    rendered = c.render({'ponyc': True, 'pony_compiler': False, 'tools': True,
                         'net_ssl': True})
    check("render", rendered,
          "ponyc=true\npony_compiler=false\ntools=true\nnet_ssl=true\n")


def test_truncation_constant():
    check("cap constant", c.FILES_API_CAP, 3000)


def run_main(stdin_text):
    """Drive main() with the given stdin; return (stdout, stderr)."""
    old_in, old_out, old_err = sys.stdin, sys.stdout, sys.stderr
    sys.stdin = io.StringIO(stdin_text)
    sys.stdout = io.StringIO()
    sys.stderr = io.StringIO()
    try:
        c.main()
        return sys.stdout.getvalue(), sys.stderr.getvalue()
    finally:
        sys.stdin, sys.stdout, sys.stderr = old_in, old_out, old_err


ALL_TRUE = ("ponyc=true\npony_compiler=true\ntools=true\nnet_ssl=true\n")


def test_main_integration():
    out, _ = run_main("src/libponyc/README.md\n")
    check("main gotcha", out,
          "ponyc=false\npony_compiler=true\ntools=false\nnet_ssl=false\n")


def test_main_empty_runs_all():
    # An empty listing (API failure, blank lines only) is never "nothing
    # changed" -- run every suite rather than report a false green.
    for label, stdin in [("empty", ""), ("blanks", "\n  \n")]:
        out, err = run_main(stdin)
        check(f"empty-{label} runs all", out, ALL_TRUE)
        check(f"empty-{label} warns", "no changed files" in err, True)


def test_main_truncation_runs_all():
    # A listing AT the cap is treated as truncated: run every suite, even though
    # these files (all excluded docs) would otherwise classify to nothing. The
    # line count is a FIXED LITERAL, never c.FILES_API_CAP -- sizing the input
    # off a mutable constant means a cap bump (or a counterfactual that mutates
    # the cap) balloons input generation into an OOM. test_truncation_constant
    # pins the constant to this same literal, so the two cannot silently drift.
    stdin = "doc.md\n" * 3000
    out, err = run_main(stdin)
    check("truncation runs all", out, ALL_TRUE)
    check("truncation warns", "API cap" in err, True)


TESTS = [test_single_path_table, test_or_aggregation, test_empty_changeset,
         test_ponyc_subset_of_tools, test_net_ssl_subset_of_ponyc,
         test_render_format, test_truncation_constant, test_main_integration,
         test_main_empty_runs_all, test_main_truncation_runs_all]


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
