## Protect pony-lint against oversize ignore files

pony-lint now rejects `.gitignore` and `.ignore` files larger than 64 KB. With hierarchical ignore loading, each directory in a project can have its own ignore files — an unexpectedly large file could cause excessive memory consumption. Ignore files that exceed the limit or that cannot be opened produce a `lint/ignore-error` diagnostic with exit code 2.
