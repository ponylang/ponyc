use ".."
use "collections"
use "pony_compiler"

primitive _WorkspaceWalk
  """
  Walks every compiled module AST across all tracked packages, applying an
  ASTVisitor to each.

  Four things to know:

  - **Uncompiled packages are skipped.** Packages whose `PackageState.package()`
    returns None (not yet compiled) are silently omitted.
  - **Foreign-source children are filtered.** `AST.visit` does not descend into
    children whose source differs from the visited module — grafted trait bodies
    and inlined type aliases are not re-visited here.
  - **Module order is undefined.** Packages are stored in a hash map; do not
    rely on visit order across modules.
  - **`Stop` is module-scoped.** Returning `Stop` from the visitor aborts the
    current module's traversal only; the walk continues with the next module.
  """
  fun apply(packages: Map[String, PackageState] box, visitor: ASTVisitor ref) =>
    for pkg_state in packages.values() do
      match pkg_state.package()
      | let pkg: Package =>
        for module in pkg.modules() do
          module.ast.visit(visitor)
        end
      end
    end
