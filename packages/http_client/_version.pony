interface val _Version is (Equatable[Version] & Stringable)

primitive HTTP10 is _Version
  """
  HTTP/1.0 protocol version.
  """
  fun string(): String => "HTTP/1.0"
  fun eq(that: Version): Bool => that is this

primitive HTTP11 is _Version
  """
  HTTP/1.1 protocol version.
  """
  fun string(): String => "HTTP/1.1"
  fun eq(that: Version): Bool => that is this
