primitive DiscardLog
  """
  Doesn't log anything.
  """
  fun val apply(ip: String, transferred: USize,
    request: Payload val, response: Payload val)
  =>
    None
