# .ci-scripts

Helper scripts that CI workflows call (Python and shell). Not part of the ponyc
build, the runtime, or the stdlib — these only run in GitHub Actions (and locally
when you want to exercise them).

## Python conventions

- **Stdlib only.** CI runners have no `pip install` step, so scripts must not
  import third-party packages. Use `urllib`, `json`, `subprocess`, `tarfile`,
  etc. — never `requests`, `pyyaml`, `pytest`, …
- **ruff lints everything here on every PR** (`.github/workflows/lint-python.yml`,
  `ruff check .ci-scripts`). There is no ruff config file, so the defaults apply.
- **Tests are `*_test.py` files, self-contained, no pytest.** Each is runnable as
  `python3 .ci-scripts/<path>/<name>_test.py`, exits 0 on pass / 1 on fail, and
  prints a one-line summary (see `pr_changed_suites_test.py` for the shape). The
  **`test` job in `lint-python.yml` discovers every `.ci-scripts/**/*_test.py`
  (via `find … -name '*_test.py'`) and runs each one**, so a new `*_test.py` is
  picked up automatically — you do not register it
  anywhere. When you add or change a script, add or update its `*_test.py`; it
  will run in CI on the next PR.
- After writing a test, break each assertion once to confirm it actually fires
  (counterfactual check) before trusting it.

## Subdirectory and import conventions

- Related scripts are grouped in a subdirectory (e.g. `libs-cache/`). A script
  imports its **siblings by module name** (`import cache_arch`); this resolves
  because the scripts are invoked by full path, so `sys.path[0]` is the script's
  own directory. **Move a script together with its siblings and its importers**,
  or the imports break.
- A subdirectory mixes **entry scripts** (runnable by full path, each with a
  `main` and a CLI) with **support-library modules** (import-only, no `main`). In
  `libs-cache/` the library modules are `common.py` (die/info/`require_env`/the
  orchestrator helpers), `registry.py` (the GHCR registry-v2 push/pull client),
  `ghpackages.py` (the GitHub-packages REST client), and `cache_arch.py` (arch
  normalization); the entry scripts (`*_libs_cache.py`, `resolve_libs_cache.py`)
  are thin layers over them. Shared logic belongs in a library module, not copied
  across entry scripts.
- A module that derives the **repo root** from `__file__` (e.g.
  `libs-cache/registry.py`'s `repo_root()`) hardcodes the directory depth with a
  fixed number of `os.path.dirname` calls. If you move it to a different depth,
  update that count — `libs-cache/repo_root_test.py` pins it, so a wrong depth
  fails the test job rather than only surfacing at runtime in CI.

## Pointers

- The GHCR libs cache (the `libs-cache/` scripts: warmer, consumers, branch
  cache, retention, clear) is documented in `.github/workflows/AGENTS.md` under
  "Known Couplings".
