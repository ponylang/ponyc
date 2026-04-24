# ponyc

## Tool Source Locations
- **pony-lint**: `tools/pony-lint/` — Pony linter
- **pony-lsp**: `tools/pony-lsp/` — Pony language server
- **pony-doc**: `tools/pony-doc/` — Pony documentation generator

## Make Commands
All test commands are slow (full rebuild + test run). Run each command at most once per code change — never re-run a command if the code has not changed since the last run.

- `make test-pony-compiler`
- `make test-pony-lsp` / `make lint-pony-lsp`
- `make test-pony-lint` / `make lint-pony-lint`
- `make test-pony-doc` / `make lint-pony-doc`

## Release Notes

Use the `/pony-release-notes` skill if available. Otherwise: create `.release-notes/<slug>.md` (e.g., `fix-foo.md`). Do NOT edit `next-release.md` directly — CI aggregates the individual files. Format: `## Title` (matching the PR title exactly) followed by a user-facing description. Include code examples for new API; include before/after for breaking changes.

## Known Couplings

Non-obvious dependencies between areas of the codebase. When changing code in one of these areas, run the listed test suites before opening a PR — failures here won't be caught by tests local to the area you changed.

- **Core AST / token definitions**: Changes to token definitions (`token.h`), AST node types, the lexer, parser, AST manipulation, sugar/desugaring passes, or subtype checking can break the tools and pony-compiler tests. Adding or reordering token enum values renumbers every subsequent token and silently breaks any code matching on specific `TK_*` types. Run: `make test-pony-compiler`, `make test-pony-lint`, `make test-pony-lsp`, `make test-pony-doc`.
