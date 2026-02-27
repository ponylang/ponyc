"""
pony-lint: A linter for Pony source files.

The linter discovers `.pony` files in target directories, runs text-based and
AST-based rules against each file, applies suppression comments, and produces
sorted diagnostics. The processing pipeline is:

**Text phase:**

1. File discovery -- recursively find `.pony` files, skipping `_corral/`,
   `_repos/`, and dot-directories.
2. Source loading -- read each file into a `SourceFile`, normalizing line
   endings.
3. Suppression parsing -- scan for `// pony-lint:` directives that suppress
   or allow specific rules or categories.
4. Text rule checking -- run each enabled `TextRule` against the source,
   producing `Diagnostic` values.
5. Filtering -- remove diagnostics on magic comment lines and in suppressed
   regions.

**AST phase:**

6. Package grouping -- group discovered files by directory.
7. AST compilation -- compile each package at `PassParse` via pony_compiler
   to obtain the parsed syntax tree.
8. AST dispatch -- walk each module's AST, dispatching nodes to matching
   `ASTRule` implementations via `_ASTDispatcher`.
9. Module-level rules -- run file-naming and blank-lines checks using entity
   info collected during the AST walk.
10. Package-level rules -- run package-naming and package-docstring checks
    once per package.

**Output:**

11. Sort diagnostics by file, line, and column, then print to stdout.

To add a new text-based rule, create a primitive implementing `TextRule` and
register it in `Main.create()`. To add a new AST-based rule, create a
primitive implementing `ASTRule` with a `node_filter()` specifying which token
types to inspect, and register it in `Main.create()`. Both rule types are
stateless -- each receives its input and returns diagnostics without side
effects.

Configuration is loaded from CLI flags and an optional `.pony-lint.json` file.
Rules can be disabled by rule ID (e.g., `style/line-length`) or by category
(e.g., `style`). CLI flags take precedence over file configuration.

Inline suppression comments let authors silence diagnostics for specific lines
or regions using `// pony-lint: off`, `// pony-lint: on`, and
`// pony-lint: allow` directives.
"""
