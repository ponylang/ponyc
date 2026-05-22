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

Non-obvious dependencies between areas of the codebase. When changing code in one of these areas, run the listed test suites before opening a PR — failures here won't be caught by tests local to the area you changed.

- **Core AST / token definitions**: Changes to token definitions (`token.h`), AST node types, the lexer, parser, AST manipulation, sugar/desugaring passes, or subtype checking can break the tools and pony-compiler tests. Adding or reordering token enum values renumbers every subsequent token and silently breaks any code matching on specific `TK_*` types. Run: `make test-pony-compiler`, `make test-pony-lint`, `make test-pony-lsp`, `make test-pony-doc`.
