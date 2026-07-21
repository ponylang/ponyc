primitive SignalAuth
  """
  Authority to create signal handlers, raise signals, and dispose of signal
  handlers.

  Derived directly from `AmbientAuth`, with no intermediate grouping
  capability — signals are a resource category of their own.
  """
  new create(from: AmbientAuth) =>
    None
