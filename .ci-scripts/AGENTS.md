# .ci-scripts

## Python conventions

- **Stdlib only.** CI runners have no `pip install` step, so scripts must not
  import third-party packages. Use `urllib`, `json`, `subprocess`, `tarfile`,
  etc. — never `requests`, `pyyaml`, `pytest`, …
- **Use ruff defaults.** There is no repository ruff configuration. From the
  repository root, run `ruff check .ci-scripts` and `ruff check test/rt-stress`.
  These Python conventions also apply to the `test/rt-stress/` orchestrator.
- **Write self-contained `*_test.py` tests without pytest.** Run each test with
  `python3 <path>/<name>_test.py`. Each test must exit 0 on pass or 1 on fail and
  print a one-line summary. When adding or changing a script, add or update its
  `*_test.py`. New `*_test.py` files are discovered automatically; do not register
  them anywhere. To run all tests under `.ci-scripts/` from the repository root:

  ```bash
  find .ci-scripts -name '*_test.py' -print0 | xargs -0 -n 1 python3
  ```
- After writing a test, break each assertion once to confirm it actually fires
  (counterfactual check) before trusting it.

## Subdirectory and import conventions

- Related scripts are grouped in a subdirectory (e.g. `libs-cache/`). A script
  imports its **siblings by module name** (`import cache_arch`); this resolves
  because the scripts are invoked by full path, so `sys.path[0]` is the script's
  own directory. **Move a script together with its siblings and its importers**,
  or the imports break.
- A subdirectory mixes **entry scripts** (runnable by full path, each with a
  `main` and a CLI) with **support-library modules** (import-only, no `main`).
  Shared logic belongs in a library module, not copied across entry scripts.
- When moving a module that derives the **repo root** from `__file__`, update
  any fixed directory-depth count to match the new location.
- Use `windows-install-deps.ps1` for every Windows job needing lldb. Keep the
  lldb pin and package installation in that script; don't inline `pacman`
  installation commands into workflows.

## Pointers

- See `libs-cache/README.md` for libs-cache usage and BSD VM provisioning.
