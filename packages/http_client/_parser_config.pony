class val _ParserConfig
  """
  Configuration for HTTP response parser size limits.

  All limits are in bytes. Responses exceeding any limit produce a parse error.
  """
  let max_status_line_size: USize
  let max_header_size: USize
  let max_chunk_header_size: USize
  let max_body_size: USize

  new val create(
    max_status_line_size': USize = 8192,
    max_header_size': USize = 8192,
    max_chunk_header_size': USize = 128,
    max_body_size': USize = 10_485_760)
  =>
    max_status_line_size = max_status_line_size'
    max_header_size = max_header_size'
    max_chunk_header_size = max_chunk_header_size'
    max_body_size = max_body_size'
