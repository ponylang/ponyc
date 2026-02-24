"""
pony-lint: A text-based linter for Pony source files.

The linter discovers `.pony` files in target directories, runs enabled rules
against each file, applies suppression comments, and produces sorted
diagnostics. The processing pipeline is:

1. File discovery -- recursively find `.pony` files, skipping `_corral/`,
   `_repos/`, and dot-directories.
2. Source loading -- read each file into a `SourceFile`, normalizing line
   endings.
3. Suppression parsing -- scan for `// pony-lint:` directives that suppress
   or allow specific rules or categories.
4. Rule checking -- run each enabled `TextRule` against the source, producing
   `Diagnostic` values.
5. Filtering -- remove diagnostics on magic comment lines and in suppressed
   regions.
6. Output -- sort diagnostics by file, line, and column, then print to stdout.

To add a new text-based rule, create a primitive implementing `TextRule` and
register it in `Main.create()`. Rules are stateless -- each receives a
`SourceFile val` and returns diagnostics without side effects.

Configuration is loaded from CLI flags and an optional `.pony-lint.json` file.
Rules can be disabled by rule ID (e.g., `style/line-length`) or by category
(e.g., `style`). CLI flags take precedence over file configuration.

Inline suppression comments let authors silence diagnostics for specific lines
or regions using `// pony-lint: off`, `// pony-lint: on`, and
`// pony-lint: allow` directives.
"""
