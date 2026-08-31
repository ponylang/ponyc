"""
pony-doc: The documentation generator for Pony source files.

Generates documentation from a compiled Pony program. Two output formats are
available: MkDocs-compatible markdown (default) and self-contained HTML.

**Pipeline:**

1. **Argument parsing** — parse CLI options for output directory, format,
   privacy settings, and target package path.
2. **Package path setup** — locate the ponyc standard library relative to the
   executable location, then append PONYPATH entries.
3. **Compilation** — compile the target package through the traits pass via
   pony_compiler, producing an AST with trait methods resolved.
4. **Extraction** — walk the compiled program AST and build a documentation IR
   (`DocProgram`) containing packages, entities, methods, fields, type
   parameters, and source locations.
5. **Generation** — pass the IR to the selected backend.

**MkDocs output** (`--format mkdocs`, default):

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

Ready for `mkdocs build` or `mkdocs serve` with the Material theme installed.

**HTML output** (`--format html`):

```
<program>-docs/
  index.html                  (package listing)
  <tqfn>.html                 (one per type, with member TOC)
  <tqfn>--index.html          (one per package)
  src/<package>/<file>.html   (source code with line numbers)
  assets/
    style.css
    logo.png
```

Open `index.html` directly in a browser — no build step needed.
"""
