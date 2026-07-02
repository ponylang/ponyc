class _CrossUsage
  """
  Uses _CrossTarget from _cross_target.pony to exercise cross-file goto
  definition navigation.

  To manually test:
  1. Open the lsp/test/workspace directory in your editor.
  2. Open this file with the Pony language server active.
  3. Place your cursor on `_CrossTarget` in the parameter type — navigates to
     _cross_target.pony, the class declaration.
  4. Place your cursor on `get` in the method call — navigates to
     _cross_target.pony, the method declaration.
  """
  fun demo(src: _CrossTarget): U32 =>
    src.get()
