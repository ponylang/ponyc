primitive TerminalAuth
  """
  Authority to change terminal mode settings.

  Derived directly from `AmbientAuth`, with no intermediate grouping
  capability.
  """
  new create(from: AmbientAuth) =>
    None
