## Fix tool install with `use` flags

When building ponyc with `use` flags (e.g., `use=pool_memalign`), `make install` failed because the tools (pony-lsp, pony-lint, pony-doc) were placed in a different output directory than ponyc. Tools now use the same suffixed output directory, and the install target handles missing tools gracefully.
