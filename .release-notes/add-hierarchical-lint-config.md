## Add hierarchical configuration for pony-lint

pony-lint now supports `.pony-lint.json` files in subdirectories, not just at the project root. A subdirectory config overrides the root config for all files in that subtree, using the same JSON format.

For example, to turn off the `style/package-docstring` rule for everything under your `examples/` directory, add an `examples/.pony-lint.json`:

```json
{"rules": {"style/package-docstring": "off"}}
```

Precedence follows proximity — the nearest directory with a setting wins. Category entries (e.g., `"style": "off"`) override parent rule-specific entries in that category. Omitting a rule from a subdirectory config defers to the parent, not the default.

Malformed subdirectory configs produce a `lint/config-error` diagnostic and fall through to the parent config — the subtree is still linted, just with the parent's rules.
