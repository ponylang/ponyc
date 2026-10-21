primitive InvalidPort is Stringable
  """
  Port is non-numeric or exceeds U16 range.
  """
  fun string(): String iso^ => "InvalidPort".clone()

primitive InvalidHost is Stringable
  """
  Malformed host: unmatched brackets, illegal characters in an IPv6
  address or IPvFuture literal, or a reg-name containing ':'.
  """
  fun string(): String iso^ => "InvalidHost".clone()

type URIParseError is (InvalidPort | InvalidHost)
