class _Tuple
  """
  Demonstrates tuple element reference goto definition.

  When accessing a tuple element via `._1`, `._2`, etc., goto definition
  resolves to the declaration of the tuple variable itself, not to the element
  type.

  To manually test:
  1. Open the lsp/test/workspace directory as a project in your editor.
  2. Open this file while the Pony language server is active.
  3. Place your cursor on `_1` in `pair._1` — navigates to the `let pair`
     declaration on the line above.
  """
  fun demo(): U32 =>
    let pair: (U32, String) = (1, "hello")
    pair._1
