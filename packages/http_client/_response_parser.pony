class _ResponseParser
  """
  HTTP/1.1 response parser.

  Data is fed in as chunks via `parse()` (matching net's delivery model).
  Parsed responses are delivered via the `_ResponseParserNotify` callback
  interface. The parser handles arbitrary chunk boundaries, connection reuse
  (multiple responses on the same connection), and both fixed-length, chunked,
  and close-delimited transfer encoding.

  The `method` field stores the request method for the response currently
  being parsed. HEAD responses and 204/304 responses have no body regardless
  of headers — the method is needed to detect HEAD.

  Fields are public so that state classes (in the same package) can access
  them for buffer reading, position tracking, state transitions, and handler
  callbacks.
  """
  var state: _ParserState
  let handler: _ResponseParserNotify ref
  let config: _ParserConfig
  var buf: Array[U8] ref = Array[U8]
  var pos: USize = 0
  var method: Method = GET
  var _failed: Bool = false

  new create(
    handler': _ResponseParserNotify ref,
    config': _ParserConfig = _ParserConfig)
  =>
    handler = handler'
    config = config'
    state = _ExpectStatusLine(config)

  fun ref expect_response(method': Method) =>
    """
    Prepare the parser to receive a response for the given request method.

    Sets the method (needed for HEAD body suppression) and resets state to
    `_ExpectStatusLine`. Called by `HTTPClientConnection.send_request()`.
    """
    method = method'
    state = _ExpectStatusLine(config)

  fun ref parse(data: Array[U8] iso) =>
    """
    Feed data to the parser.

    The parser processes as much data as possible in a single call,
    delivering callbacks for each complete response (or response component)
    found. Remaining partial data is buffered for the next call.
    """
    if _failed then return end

    buf.append(consume data)

    var continue_parsing = true
    while continue_parsing do
      match \exhaustive\ state.parse(this)
      | _ParseContinue =>
        if _failed then break end
      | _ParseNeedMore => continue_parsing = false
      | let err: ParseError =>
        handler.parse_error(err)
        _failed = true
        continue_parsing = false
      end
    end

    // Compact consumed data
    if pos > 0 then
      buf.trim_in_place(pos)
      pos = 0
    end

  fun ref connection_closed() =>
    """
    Signal that the remote end closed the connection.

    If the parser is currently in `_ExpectCloseDelimitedBody` state, this
    completes the response by calling `handler.response_complete()`. For
    all other states, this is a no-op — the connection class handles
    `on_closed()` separately.
    """
    if _failed then return end
    match state
    | let _: _ExpectCloseDelimitedBody =>
      handler.response_complete()
      _failed = true
    end

  fun ref stop() =>
    """
    Stop the parser. All subsequent `parse()` calls become no-ops.

    Safe to call from within a handler callback during parsing — the parse
    loop checks the failed flag after each state transition.
    """
    _failed = true

  fun ref extract_bytes(from: USize, to: USize): Array[U8] iso^ =>
    """
    Copy bytes from buf[from..to) into a new iso array.
    """
    let len = to - from
    let out = recover Array[U8].create(len) end
    var i = from
    while i < to do
      try out.push(buf(i)?) else _Unreachable() end
      i = i + 1
    end
    out

  fun ref extract_string(from: USize, to: USize): String iso^ =>
    """
    Copy bytes from buf[from..to) into a new iso String.
    """
    let len = to - from
    let out = recover String.create(len) end
    var i = from
    while i < to do
      try out.push(buf(i)?) else _Unreachable() end
      i = i + 1
    end
    out
