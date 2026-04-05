## Protect pony-lint against oversize configuration files

pony-lint now rejects `.pony-lint.json` files larger than 64 KB. With hierarchical configuration, each directory in a project can have its own config file — an unexpectedly large file could cause excessive memory consumption. Config files that exceed the limit produce a `lint/config-error` diagnostic with the file size and path.
