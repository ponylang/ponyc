primitive InvalidScheme is Stringable
  """
  The scheme contains characters not allowed by RFC 3986 section 3.1.
  """
  fun string(): String =>
    "invalid scheme"

primitive InvalidDotSegment is Stringable
  """
  A dot segment (`.` or `..`) was passed to `append_path_segment`.

  Dot segments are special path components that `RemoveDotSegments`,
  `NormalizeURI`, and `ResolveURI` treat as relative navigation. No
  percent-encoding can preserve them as literal segments — RFC 3986 §6.2.2.2
  normalizes `%2E` back to `.` before dot-segment removal runs.
  """
  fun string(): String =>
    "dot segment in append_path_segment"

type URIBuildError is (InvalidScheme | InvalidDotSegment | URIParseError)
