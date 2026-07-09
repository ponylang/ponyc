# ponyc

<!-- contributor-only -->
## Contributing with an AI assistant

This is a Pony project. The ponylang org maintains a set of LLM coding skills. Get set up with them before contributing:

- **Not set up yet?** Install them once:

  ```bash
  git clone https://github.com/ponylang/llm-skills.git
  cd llm-skills
  python install.py
  ```

- **Already set up?** Make sure you're on the latest. If you installed with the script above, `git pull` in the directory where you cloned `llm-skills` and the symlinked skills update automatically — if you set them up another way, refresh them however that setup expects.

See the [llm-skills README](https://github.com/ponylang/llm-skills) for details and other harnesses.

When you start working on this project, load the `pony-skills` skill — it tells your assistant which Pony skill to use for each task.

Read [CONTRIBUTING.md](CONTRIBUTING.md).
<!-- /contributor-only -->

## Tool Source Locations
- **pony-lint**: `tools/pony-lint/` — Pony linter
- **pony-lsp**: `tools/pony-lsp/` — Pony language server
- **pony-doc**: `tools/pony-doc/` — Pony documentation generator

## Tool Test and Lint Commands

The four self-hosted tools' test binaries are built on demand — a normal `cmake --build` does not build them. On Linux/macOS, build the test binary, then run it through ctest:

- `cmake --build --preset debug --target pony-compiler-tests && ctest --preset debug -R pony-compiler-tests`
- `cmake --build --preset debug --target pony-lsp-tests && ctest --preset debug -R pony-lsp-tests`
- `cmake --build --preset debug --target pony-lint-tests && ctest --preset debug -R pony-lint-tests`
- `cmake --build --preset debug --target pony-doc-tests && ctest --preset debug -R pony-doc-tests`

`cmake --build` is incremental: the first build of a test binary compiles from Pony source (~60s); later runs skip the rebuild when nothing under its source tree changed. On Windows, build the target and run it with ctest the same way, using the `windows-x86-64` presets: `cmake --build --preset windows-x86-64-debug --target pony-lint-tests` then `ctest --preset windows-x86-64-debug -R pony-lint-tests`.

To lint a tool's own source, run the `pony-lint` binary (built by a normal `cmake --build --preset debug`) against the tool's directory:

- `cd build/debug && PONYPATH=../../tools/lib/ponylang/pony_compiler ./pony-lint ../../tools/pony-lsp/`
- `cd build/debug && PONYPATH=../../tools/lib/ponylang/pony_compiler ./pony-lint ../../tools/pony-lint/`
- `cd build/debug && PONYPATH=../../tools/lib/ponylang/pony_compiler ./pony-lint ../../tools/pony-doc/`

## Release Notes

Use the `/pony-release-notes` skill if available. Otherwise: create `.release-notes/<slug>.md` (e.g., `fix-foo.md`). Do NOT edit `next-release.md` directly — CI aggregates the individual files. Format: `## Title` (matching the PR title exactly) followed by a user-facing description. Include code examples for new API; include before/after for breaking changes.
