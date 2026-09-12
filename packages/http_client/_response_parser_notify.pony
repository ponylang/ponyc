trait ref _ResponseParserNotify
  """
  Callback interface for the HTTP response parser.
  """

  fun ref response_received(
    status: U16,
    reason: String val,
    version: Version,
    headers: Headers val)
    """
    Called when the status line and all headers have been parsed.

    For responses with a body (Content-Length, chunked, or close-delimited),
    `body_chunk` calls follow. For responses without a body (HEAD, 204, 304),
    `response_complete` is called immediately after.
    """

  fun ref body_chunk(data: Array[U8] val)
    """
    Called for each chunk of response body data as it becomes available.

    Body data is delivered incrementally -- not accumulated.
    """

  fun ref response_complete()
    """
    Called when the entire response (including any body) has been received.

    After this call, the parser is ready to parse the next response on the
    same connection (keep-alive).
    """

  fun ref parse_error(err: ParseError)
    """
    Called when a parse error is encountered.

    After this call, the parser enters a terminal failed state and will not
    produce any further callbacks. The connection should be closed.
    """
