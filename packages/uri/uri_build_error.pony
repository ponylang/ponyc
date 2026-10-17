primitive InvalidScheme is Stringable
  """
  The scheme contains characters not allowed by RFC 3986 section 3.1.
  """
  fun string(): String =>
    "invalid scheme"

type URIBuildError is (InvalidScheme | URIParseError)
