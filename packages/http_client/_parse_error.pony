primitive TooLarge
  """
  Status line or headers exceed the configured size limit.
  """

primitive InvalidStatusLine
  """
  Response status line is malformed.
  """

primitive InvalidVersion
  """
  HTTP version is not HTTP/1.0 or HTTP/1.1.
  """

primitive MalformedHeaders
  """
  Header syntax is invalid (missing colon, obs-fold continuation line).
  """

primitive InvalidContentLength
  """
  Content-Length is non-numeric, negative, or has conflicting values.
  """

primitive InvalidChunk
  """
  Chunked transfer encoding error: bad chunk size or missing CRLF.
  """

primitive BodyTooLarge
  """
  Response body exceeds the configured maximum body size.
  """
