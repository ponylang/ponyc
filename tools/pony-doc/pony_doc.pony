"""
pony-doc: A documentation generator for Pony source files.

Generates MkDocs-compatible documentation from a compiled Pony program,
producing output identical to ponyc's built-in `--docs` flag. The tool is
distributed alongside ponyc and shares its version number.

**Pipeline:**

1. **Argument parsing** — parse CLI options for output directory, privacy
   settings, and target package path.
2. **Package path setup** — locate the ponyc standard library relative to the
   executable location, then append PONYPATH entries.
3. **Compilation** — compile the target package through the traits pass via
   pony_compiler, producing a typed AST.
4. **Extraction** — walk the compiled program AST and build a documentation IR
   (`DocProgram`) containing packages, entities, methods, fields, type
   parameters, and source locations.
5. **Generation** — pass the IR to the MkDocs backend, which writes mkdocs.yml,
   package index pages, entity type pages, source code pages, and theme assets.

**Output structure:**

```
<program>-docs/
  mkdocs.yml
  docs/
    index.md                  (package listing)
    <tqfn>.md                 (one per type)
    <tqfn>--index.md          (one per package)
    src/<package>/<file>.md   (source code)
    assets/
      ponylang.css
      logo.png
```

The output is ready for `mkdocs build` or `mkdocs serve` with the Material
theme installed.
"""
