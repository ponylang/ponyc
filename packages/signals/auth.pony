primitive SignalAuth
  """
  Authority to create signal handlers, raise signals, and dispose of signal
  handlers.

  This is derived directly from `AmbientAuth`. There is no intermediate
  grouping — signals are a distinct resource category.
  """
  new create(from: AmbientAuth) =>
    None
