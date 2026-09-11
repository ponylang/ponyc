primitive InvalidPort is Stringable
  """
  Port is non-numeric or exceeds U16 range.
  """
  fun string(): String iso^ => "InvalidPort".clone()

primitive InvalidHost is Stringable
  """
  Malformed IP-literal (unmatched brackets, illegal characters in IPv6
  address, or invalid IPvFuture syntax).
  """
  fun string(): String iso^ => "InvalidHost".clone()

type URIParseError is (InvalidPort | InvalidHost)
