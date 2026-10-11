use ".."
use "collections"
use "pony_compiler"

primitive _WorkspaceWalk
  """
  Walks every compiled file AST across all tracked modules, applying an
  ASTVisitor to each.

  Four things to know:

  - **Uncompiled modules are skipped.** Modules whose `ModuleState.module()`
    returns None (not yet compiled) are silently omitted.
  - **Foreign-source children are filtered.** `AST.visit` does not descend into
    children whose source differs from the visited file — grafted trait bodies
    and inlined type aliases are not re-visited here.
  - **File order is undefined.** Modules are stored in a hash map; do not
    rely on visit order across files.
  - **`Stop` is file-scoped.** Returning `Stop` from the visitor aborts the
    current file's traversal only; the walk continues with the next file.
  """
  fun apply(modules: Map[String, ModuleState] box, visitor: ASTVisitor ref) =>
    for mod_state in modules.values() do
      match mod_state.module()
      | let mod: Module =>
        for f in mod.files() do
          f.ast.visit(visitor)
        end
      end
    end
