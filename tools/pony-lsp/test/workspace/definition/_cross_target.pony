class _CrossTarget
  """
  A class defined in a separate file to exercise cross-file goto definition
  navigation. This file is referenced from _cross_usage.pony.

  To manually test:
  1. Open the lsp/test/workspace directory in your editor.
  2. Open _cross_usage.pony with the Pony language server active.
  3. In that file, place your cursor on `_CrossTarget` in the parameter type
     — navigates here, to the class declaration.
  4. Place your cursor on `get` in the method call — navigates here, to the
     method declaration.
  """
  fun get(): U32 => 0
