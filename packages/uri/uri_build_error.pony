primitive InvalidScheme is Stringable
  """
  The scheme contains characters not allowed by RFC 3986 section 3.1.
  """
  fun string(): String iso^ =>
    "invalid scheme".clone()

type URIBuildError is (InvalidScheme | URIParseError)
