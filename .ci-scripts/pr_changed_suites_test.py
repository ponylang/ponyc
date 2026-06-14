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


# (path, expected (ponyc, pony_compiler, tools)) -- one row per distinguishing
# case. Each suite predicate is exercised at every boundary it draws.
SINGLE_PATH_TABLE = [
    # runtime code: ponyc + tools, but not the compiler suite.
    ('src/libponyrt/foo.c', (True, False, True)),
    # libponyc source triggers all three.
    ('src/libponyc/foo.c', (True, True, True)),
    # THE GOTCHA: a libponyc doc/yaml is excluded from ponyc and tools (doc/yaml
    # filtered) but still triggers pony_compiler (its filter has no exclusions).
    ('src/libponyc/README.md', (False, True, False)),
    ('src/libponyc/notes.yml', (False, True, False)),
    # tools code that is NOT the pony_compiler library: tools only.
    ('tools/pony-lint/config.pony', (False, False, True)),
    # the pony_compiler library lives under tools/, so ponyc (no tools/) skips
    # it while pony_compiler and tools both run.
    ('tools/lib/ponylang/pony_compiler/pass.pony', (False, True, True)),
    # plain stdlib code: ponyc + tools.
    ('packages/json/json.pony', (True, False, True)),
    # excluded files trigger nothing (and are not pony_compiler paths).
    ('README.md', (False, False, False)),
    ('config.yaml.yml', (False, False, False)),
    ('.dockerfiles/x86-64-unknown-linux/Dockerfile', (False, False, False)),
    ('.ci-dockerfiles/build.sh', (False, False, False)),
    ('.gitignore', (False, False, False)),
    ('.markdownlintignore', (False, False, False)),
    # the merged workflow file re-includes into every suite.
    ('.github/workflows/pr.yml', (True, True, True)),
    # any OTHER workflow yaml is excluded everywhere (yaml filter, not the WF).
    ('.github/workflows/release.yml', (False, False, False)),
]


def test_single_path_table():
    for path, want in SINGLE_PATH_TABLE:
        result = c.classify([path])
        got = (result['ponyc'], result['pony_compiler'], result['tools'])
        check(f"classify([{path!r}])", got, want)


def test_or_aggregation():
    # A suite is on if ANY changed file triggers it.
    result = c.classify(['README.md', 'src/libponyc/foo.c'])
    check("aggregate ponyc", result['ponyc'], True)
    check("aggregate pony_compiler", result['pony_compiler'], True)
    check("aggregate tools", result['tools'], True)
    # Doc-only change triggers nothing.
    docs = c.classify(['README.md', 'docs/guide.md'])
    check("docs-only", docs, {'ponyc': False, 'pony_compiler': False,
                              'tools': False})


def test_empty_changeset():
    check("empty", c.classify([]),
          {'ponyc': False, 'pony_compiler': False, 'tools': False})


def test_ponyc_subset_of_tools():
    # ponyc must be a subset of tools, so the workflow-level union (tools plus
    # pony_compiler) never skips a ponyc-triggering file.
    for path, _ in SINGLE_PATH_TABLE:
        r = c.classify([path])
        if r['ponyc']:
            check(f"{path!r} ponyc=>tools", r['tools'], True)


def test_render_format():
    rendered = c.render({'ponyc': True, 'pony_compiler': False, 'tools': True})
    check("render", rendered, "ponyc=true\npony_compiler=false\ntools=true\n")


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


def test_main_integration():
    out, _ = run_main("src/libponyc/README.md\n")
    check("main gotcha", out, "ponyc=false\npony_compiler=true\ntools=false\n")
    out, _ = run_main("\n  \n")  # blank lines are stripped -> empty changeset
    check("main empty", out, "ponyc=false\npony_compiler=false\ntools=false\n")


def test_main_truncation_runs_all():
    # A listing AT the cap is treated as truncated: run every suite, even though
    # these files (all excluded docs) would otherwise classify to nothing. The
    # line count is a FIXED LITERAL, never c.FILES_API_CAP -- sizing the input
    # off a mutable constant means a cap bump (or a counterfactual that mutates
    # the cap) balloons input generation into an OOM. test_truncation_constant
    # pins the constant to this same literal, so the two cannot silently drift.
    stdin = "doc.md\n" * 3000
    out, err = run_main(stdin)
    check("truncation runs all", out,
          "ponyc=true\npony_compiler=true\ntools=true\n")
    check("truncation warns", "API cap" in err, True)


TESTS = [test_single_path_table, test_or_aggregation, test_empty_changeset,
         test_ponyc_subset_of_tools, test_render_format,
         test_truncation_constant, test_main_integration,
         test_main_truncation_runs_all]


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
