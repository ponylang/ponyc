primitive _Color
  """
  Strings to embedded in text to specify colours.
  These are copies of the strings defined in packages/term. They are duplicated
  here to avoid a dependency.
  """
  fun reset(): String =>
    """
    Resets all colours and text styles to the default.
    """
    "\x1B[0m"

  fun red(): String =>
    """
    Bright red text.
    """
    "\x1B[91m"

  fun green(): String =>
    """
    Bright green text.
    """
    "\x1B[92m"
