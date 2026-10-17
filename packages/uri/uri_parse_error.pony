primitive InvalidPort is Stringable
  """
  Port is non-numeric or exceeds U16 range.
  """
  fun string(): String => "InvalidPort"

primitive InvalidHost is Stringable
  """
  Malformed host: unmatched brackets, an IPv6 address that does not
  match the RFC 3986 grammar, a malformed IPvFuture literal, or a
  reg-name containing ':'.
  """
  fun string(): String => "InvalidHost"

type URIParseError is (InvalidPort | InvalidHost)
