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

## Make Commands
On Linux/macOS (via `make`), the first invocation of each command rebuilds its test or lint binary from Pony source (~60s); subsequent invocations skip the rebuild when nothing under the binary's source tree has changed and just run the binary, so re-running on unchanged code is cheap. The Windows `make.ps1` path always rebuilds from source.

- `make test-pony-compiler`
- `make test-pony-lsp` / `make lint-pony-lsp`
- `make test-pony-lint` / `make lint-pony-lint`
- `make test-pony-doc` / `make lint-pony-doc`

## Release Notes

Use the `/pony-release-notes` skill if available. Otherwise: create `.release-notes/<slug>.md` (e.g., `fix-foo.md`). Do NOT edit `next-release.md` directly — CI aggregates the individual files. Format: `## Title` (matching the PR title exactly) followed by a user-facing description. Include code examples for new API; include before/after for breaking changes.

## Known Couplings

Non-obvious dependencies between areas of the codebase — where a change in one place silently breaks another and the breakage isn't caught by tests local to the area you changed — are recorded one-per-file in `.known-couplings/`. Each file documents one coupling: what depends on what, why a change breaks it, and, where applicable, the test suites to run.

**Before changing code other areas depend on** — the runtime, codegen, the AST/token/`pass_id` machinery, the stdlib packages the tools consume, the build and packaging rules, or CI — read any file in `.known-couplings/` that bears on what you're touching: scan the filenames, and `grep` the directory for the file or symbol you're about to change. The list above is illustrative, not exhaustive — the directory is the authority. A break in coupled code often won't surface in the tests near your change, so run the suites named in the coupling file before opening a PR.

**When you discover or introduce a coupling**, add a new file `.known-couplings/<slug>.md` — a kebab-case slug naming the specific coupling, not the broad area (one area often holds several couplings, and they must never share a file). One coupling per file; never append a second coupling to an existing file — separate files are what keep this list from being a perpetual merge-conflict (the reason it was split out of this document), and consolidating them back together would bring the conflicts back. Format: a `# Title` heading, then a body explaining the dependency and how a change breaks it, ending where applicable with a `Run:` line naming the suites that catch a regression. Where the breakage point is in source, also flag the coupling with a short comment there — as the existing couplings do — so whoever next changes that code is warned to check `.known-couplings/`. When a change removes the underlying dependency, delete its file and that comment in the same change.
