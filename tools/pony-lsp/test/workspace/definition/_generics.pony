primitive _Generics
  """
  A generic primitive for testing type parameter goto definition.

  The `apply` method has a type parameter `T`. The return type annotation
  `: T` is a type parameter reference — goto definition on it navigates to
  the `T` declaration in the method's type parameter list (`[T]`).

  Package-qualified type references (e.g. `col.List[U32]` where
  `use col = "collections"`) also support goto definition and navigate to the
  first source file in that package — test this manually by adding a
  package-aliased import and placing the cursor on the alias name.

  To manually test:
  1. Open the lsp/test/workspace directory as a project in your editor.
  2. Open this file while the Pony language server is active.
  3. Place your cursor on `T` in the return type of `apply` — navigates to
     the `T` type parameter declaration in the method header.
  """
  fun apply[T](x: T): T =>
    x
