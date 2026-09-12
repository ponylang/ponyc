type ParseError is
  ( TooLarge
  | InvalidStatusLine
  | InvalidVersion
  | MalformedHeaders
  | InvalidContentLength
  | InvalidChunk
  | BodyTooLarge )
  """
  Parse error encountered during HTTP response parsing.
  """
