primitive DiscardLog
  """
  Doesn't log anything.
  """
  fun val apply(
    ip: String,
    body_size: USize,
    request: Payload val,
    response: Payload val)
  =>
    None
