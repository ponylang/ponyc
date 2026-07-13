#!/usr/bin/env python3
"""Classify a PR's changed files into the CI suites that must run.

The merged `pr.yml` workflow runs three suites -- `ponyc`, `pony_compiler`, and
`tools` -- but most PRs touch only some of them. This is the home-grown
paths-filter the `changes` job uses to decide which suites to start: it reads the
PR's changed paths on stdin (one per line) and writes `<suite>=true|false` for
each suite to stdout in `$GITHUB_OUTPUT` (`name=value`) format.

The rules are the three suites' original `paths:` blocks, transcribed as plain
prefix/suffix/exact tests -- no glob engine -- plus two deliberate departures
from those blocks. First, `test/rt-stress/` and `test/rt-systematic/` are
excluded from every suite. They hold test workloads the PR suites build nothing
of -- their Pony is compiled and run only by scheduled workflows (`stress-test-*`
and `ponyc-weekly-checks.yml`). The stress harness's Python orchestration is
linted by `lint-python.yml` and tested by `test-rt-stress.yml` when it changes;
`test/rt-systematic/` carries no Python (its orchestrator lives under
`.ci-scripts/`). So a PR touching only these needs none of these suites.

Second, the shared CMake build system -- the top-level `CMakeLists.txt`,
`CMakePresets.json`, and `cmake/` -- triggers `pony_compiler`. That suite builds
`pony-compiler-tests`, a target that `add_pony_binary` (in
`cmake/PonyBinary.cmake`) defines and `cmake --build --preset debug --target
pony-compiler-tests` compiles, so a build-system change can break its build.
`ponyc` and `tools` already match these files through their broad rules; only
`pony_compiler`'s allowlist would otherwise skip them. Build-system doc and yaml
files stay excluded, so this stays in sync with the union filter (below) without
a re-include.

The rules must stay in sync with the union `paths:` filter at the top of
`pr.yml`: a file that triggers any suite here must also match the workflow-level
filter, or the workflow never starts and that suite silently never runs.
(`ponyc` is a subset of `tools`, so the union is exactly `tools` plus
`pony_compiler`.)

Editing `pr.yml` itself re-triggers every suite: each suite's original filter
re-included its own workflow file, and now there is one shared file.

A `pulls/<n>/files` listing caps at 3000 files; a larger PR comes back truncated,
so a full listing of exactly the cap (or more) is treated as "run everything"
rather than risk skipping a suite whose only changed file fell past the cap.

Stdlib only; no pip install on any CI runner.
"""
import sys

FILES_API_CAP = 3000
WORKFLOW_FILE = '.github/workflows/pr.yml'


def excluded(path):
    return (path.endswith('.md') or path.endswith('.yml')
            or path.startswith('.dockerfiles/')
            or path.startswith('.ci-dockerfiles/')
            or path.startswith('test/rt-stress/')
            or path.startswith('test/rt-systematic/')
            or path == '.gitignore' or path == '.markdownlintignore')


def matches_ponyc(path):
    return (not excluded(path) and not path.startswith('tools/')) \
        or path == WORKFLOW_FILE


def matches_build_system(path):
    return not excluded(path) and (
        path == 'CMakeLists.txt'
        or path == 'CMakePresets.json'
        or path.startswith('cmake/'))


def matches_pony_compiler(path):
    return (path.startswith('src/libponyc/')
            or path.startswith('tools/lib/ponylang/pony_compiler/')
            or matches_build_system(path)
            or path == WORKFLOW_FILE)


def matches_tools(path):
    return not excluded(path) or path == WORKFLOW_FILE


SUITES = (('ponyc', matches_ponyc),
          ('pony_compiler', matches_pony_compiler),
          ('tools', matches_tools))


def classify(paths):
    return {name: any(m(p) for p in paths) for name, m in SUITES}


def render(result):
    return ''.join(f"{n}={'true' if result[n] else 'false'}\n"
                   for n, _ in SUITES)


def main():
    paths = [line.strip() for line in sys.stdin if line.strip()]
    if len(paths) >= FILES_API_CAP:                 # truncated listing -> run all
        print(f"warning: changed-file list hit the API cap ({FILES_API_CAP}); "
              "running all suites.", file=sys.stderr)
        result = {n: True for n, _ in SUITES}
    else:
        result = classify(paths)
    sys.stdout.write(render(result))


if __name__ == '__main__':
    main()
