"""
pony-doc: The documentation generator for Pony source files.

Generates documentation from a compiled Pony program using a pluggable
backend. The tool is distributed alongside ponyc and shares its version
number.

**Pipeline:**

1. **Argument parsing** — parse CLI options for output directory, backend
   selection, privacy settings, and target module path.
2. **Module path setup** — locate the ponyc standard library relative to the
   executable location, then append PONYPATH entries.
3. **Compilation** — compile the target module through the traits pass via
   pony_compiler, producing an AST with trait methods resolved.
4. **Extraction** — walk the compiled program AST and build a documentation IR
   (`DocProgram`) containing modules, entities, methods, fields, type
   parameters, and source locations.
5. **Generation** — pass the IR to the selected backend.

**Backends:**

*MkDocs* (`--backend mkdocs`, default) — writes a directory tree ready for
`mkdocs build` or `mkdocs serve` with the Material theme:

```
<program>-docs/
  mkdocs.yml
  docs/
    index.md                  (module listing)
    <tqfn>.md                 (one per type)
    <tqfn>--index.md          (one per module)
    src/<module>/<file>.md    (source code)
    assets/
      logo.png
```

*Info* (`--backend info`) — writes a single GNU Info file readable by any
Info viewer (Emacs, standalone `info`, `pinfo`):

```
<program>.info
```
"""
