"""
Test fixture: base class in a sub-module for cross-module reference tests.
"""

class RefBase
  """
  Used by ref_base_user.pony to exercise cross-module WorkspaceWalk.
  """
  fun value(): U32 => 42
