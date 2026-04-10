# ponyc

## Tool Source Locations
- **pony-lint**: `tools/pony-lint/` — Pony linter
- **pony-lsp**: `tools/pony-lsp/` — Pony language server
- **pony-doc**: `tools/pony-doc/` — Pony documentation generator

## Make Commands
Each tool has corresponding test and lint targets:
- `make test-pony-lsp` / `make lint-pony-lsp`
- `make test-pony-lint` / `make lint-pony-lint`
- `make test-pony-doc` / `make lint-pony-doc`

These commands are slow (full rebuild + test run). Run each command at most once per code change — never re-run a command if the code has not changed since the last run.
