"""
Test fixture: base class in a sub-package for cross-package reference tests.
"""

class RefBase
  """
  Used by ref_base_user.pony to exercise cross-package WorkspaceWalk.
  """
  fun value(): U32 => 42
