interface val _Version is (Equatable[Version] & Stringable)

primitive HTTP10 is _Version
  """
  HTTP/1.0 protocol version.
  """
  fun string(): String iso^ => "HTTP/1.0".clone()
  fun eq(that: Version): Bool => that is this

primitive HTTP11 is _Version
  """
  HTTP/1.1 protocol version.
  """
  fun string(): String iso^ => "HTTP/1.1".clone()
  fun eq(that: Version): Bool => that is this
