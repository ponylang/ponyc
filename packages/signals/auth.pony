primitive SignalAuth
  """
  Authority to create signal handlers, raise signals, and dispose of signal
  handlers.

  Derived directly from `AmbientAuth`, with no broader grouping
  capability in between (unlike, say, the `net` package's `NetAuth`
  hierarchy) — signals are a distinct resource category of their own.
  """
  new create(from: AmbientAuth) =>
    None
