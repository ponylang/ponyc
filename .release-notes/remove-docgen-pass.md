## Remove docgen pass

We've removed ponyc's built-in documentation generation pass. The `--docs`, `-g`, and `--docs-public` command-line flags no longer exist, and `--pass docs` is no longer a valid compilation limit.

Use `pony-doc` instead. It shipped in 0.61.0 as the replacement and has been the recommended tool since then. If you were using `--docs-public`, `pony-doc` generates public-only documentation by default. If you were using `--docs` to include private types, use `pony-doc --include-private`.
