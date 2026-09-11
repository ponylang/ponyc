primitive _ParseContinue
  """
  More data may be parseable in the current buffer.
  """

primitive _ParseNeedMore
  """
  Need more data from the network before parsing can continue.
  """

type _ParseResult is (_ParseContinue | _ParseNeedMore | ParseError)
  """
  Result of a single parse step.
  """

interface ref _ParserState
  """
  A state in the HTTP response parser state machine.

  Each state is a class that owns its per-state data (buffers,
  accumulators). State transitions are explicit assignments to
  `p.state`. Per-state data is automatically cleaned up when
  the state transitions out.
  """
  fun ref parse(p: _ResponseParser ref): _ParseResult

// ---------------------------------------------------------------------------
// Buffer scanning utilities
// ---------------------------------------------------------------------------
primitive _BufferScan
  """
  Byte-level scanning utilities for the parser buffer.
  """

  fun find_crlf(buf: Array[U8] box, from: USize = 0): (USize | None) =>
    """
    Find the position of \\r\\n in buf starting from `from`.
    Returns the index of \\r, or None if not found.
    """
    if buf.size() < (from + 2) then return None end
    var i = from
    let limit = buf.size() - 1
    try
      while i < limit do
        if (buf(i)? == '\r') and (buf(i + 1)? == '\n') then
          return i
        end
        i = i + 1
      end
    else
      _Unreachable()
    end
    None

  fun find_byte(
    buf: Array[U8] box,
    byte: U8,
    from: USize,
    to: USize = USize.max_value())
    : (USize | None)
  =>
    """
    Find the first occurrence of `byte` in buf[from, to).
    """
    var i = from
    let limit = to.min(buf.size())
    try
      while i < limit do
        if buf(i)? == byte then return i end
        i = i + 1
      end
    else
      _Unreachable()
    end
    None

// ---------------------------------------------------------------------------
// Parser states
// ---------------------------------------------------------------------------
class _ExpectStatusLine is _ParserState
  """
  Waiting for a complete HTTP status line.

  Format: HTTP-version SP status-code SP reason-phrase CRLF
  """
  let _config: _ParserConfig

  new create(config: _ParserConfig) =>
    _config = config

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    let available = p.buf.size() - p.pos

    match \exhaustive\ _BufferScan.find_crlf(p.buf, p.pos)
    | let crlf: USize =>
      let line_len = crlf - p.pos
      if line_len > _config.max_status_line_size then
        return TooLarge
      end

      // Parse version: must start with "HTTP/1.0" or "HTTP/1.1"
      if line_len < 12 then
        // Minimum: "HTTP/1.x NNN" = 12 chars
        return InvalidStatusLine
      end

      let version =
        try
          if (p.buf(p.pos)? == 'H') and
            (p.buf(p.pos + 1)? == 'T') and
            (p.buf(p.pos + 2)? == 'T') and
            (p.buf(p.pos + 3)? == 'P') and
            (p.buf(p.pos + 4)? == '/') and
            (p.buf(p.pos + 5)? == '1') and
            (p.buf(p.pos + 6)? == '.')
          then
            let minor = p.buf(p.pos + 7)?
            if minor == '1' then
              HTTP11
            elseif minor == '0' then
              HTTP10
            else
              return InvalidVersion
            end
          else
            return InvalidVersion
          end
        else
          _Unreachable()
          return InvalidVersion
        end

      // Expect space after version
      try
        if p.buf(p.pos + 8)? != ' ' then
          return InvalidStatusLine
        end
      else
        _Unreachable()
        return InvalidStatusLine
      end

      // Parse 3-digit status code at pos+9
      let status_start = p.pos + 9
      if (status_start + 3) > crlf then
        return InvalidStatusLine
      end

      let status: U16 =
        try
          let d1 = p.buf(status_start)?
          let d2 = p.buf(status_start + 1)?
          let d3 = p.buf(status_start + 2)?
          if (d1 < '0') or (d1 > '9') or
            (d2 < '0') or (d2 > '9') or
            (d3 < '0') or (d3 > '9')
          then
            return InvalidStatusLine
          end
          (((d1 - '0').u16() * 100) +
            ((d2 - '0').u16() * 10) +
            (d3 - '0').u16())
        else
          _Unreachable()
          return InvalidStatusLine
        end

      // Reason phrase: everything after status code to CRLF
      // May be empty. If present, a space separates status from reason.
      let after_status = status_start + 3
      let reason: String val =
        if after_status < crlf then
          // Skip the space between status and reason
          let reason_start =
            if
              try
                p.buf(after_status)? == ' '
              else
                _Unreachable()
                false
              end
            then
              after_status + 1
            else
              after_status
            end
          p.extract_string(reason_start, crlf)
        else
          ""
        end

      // Advance past the status line
      p.pos = crlf + 2

      // Transition to header parsing
      p.state = _ExpectHeaders(status, reason, version, _config)
      _ParseContinue

    | None =>
      // No complete line yet — check size limit
      if available > _config.max_status_line_size then
        TooLarge
      else
        _ParseNeedMore
      end
    end

class _ExpectHeaders is _ParserState
  """
  Parsing HTTP headers after the status line.

  Loops through header lines until an empty line (CRLF) marks the end
  of headers. Tracks Content-Length and Transfer-Encoding for body handling.
  Determines body framing per RFC 7230 §3.3.3 for responses.
  """
  let _status: U16
  let _reason: String val
  let _version: Version
  let _config: _ParserConfig
  var _headers: Headers iso = recover iso Headers end
  var _content_length: (USize | None) = None
  var _chunked: Bool = false
  var _total_header_bytes: USize = 0

  new create(
    status: U16,
    reason: String val,
    version: Version,
    config: _ParserConfig)
  =>
    _status = status
    _reason = reason
    _version = version
    _config = config

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    while true do
      match \exhaustive\ _BufferScan.find_crlf(p.buf, p.pos)
      | let crlf: USize =>
        if crlf == p.pos then
          // Empty line: end of headers
          p.pos = crlf + 2

          // 1xx informational: discard and await the final response
          if (_status >= 100) and (_status < 200) then
            p.state = _ExpectStatusLine(_config)
            return _ParseContinue
          end

          // Deliver the response metadata
          let headers: Headers val =
            (_headers = recover iso Headers end)
          p.handler.response_received(_status, _reason, _version, headers)

          // Body determination per RFC 7230 §3.3.3:
          // HEAD responses and 204/304 have no body regardless of headers
          if (p.method is HEAD) or (_status == 204) or (_status == 304) then
            p.handler.response_complete()
            p.state = _ExpectStatusLine(_config)
            return _ParseContinue
          end

          // Transfer-Encoding: chunked takes precedence over Content-Length
          if _chunked then
            p.state = _ExpectChunkHeader(0, _config)
            return _ParseContinue
          end

          match \exhaustive\ _content_length
          | let cl: USize if cl > 0 =>
            // Check body size limit before entering body state
            if cl > _config.max_body_size then
              return BodyTooLarge
            end
            p.state = _ExpectFixedBody(cl)
            return _ParseContinue
          | let _: USize =>
            // Content-Length: 0
            p.handler.response_complete()
            p.state = _ExpectStatusLine(_config)
            return _ParseContinue
          else
            // No Content-Length, no chunked: close-delimited body
            p.state = _ExpectCloseDelimitedBody(_config)
            return _ParseContinue
          end
        end

        // Track header size
        let line_len = (crlf - p.pos) + 2
        _total_header_bytes = _total_header_bytes + line_len
        if _total_header_bytes > _config.max_header_size then
          return TooLarge
        end

        // Check for obs-fold (continuation line): reject per RFC 7230
        try
          let first_byte = p.buf(p.pos)?
          if (first_byte == ' ') or (first_byte == '\t') then
            return MalformedHeaders
          end
        else
          _Unreachable()
        end

        // Find colon separator
        let colon_pos =
          match \exhaustive\ _BufferScan.find_byte(p.buf, ':', p.pos, crlf)
          | let i: USize => i
          | None => return MalformedHeaders
          end

        // Header name must not be empty
        if colon_pos == p.pos then
          return MalformedHeaders
        end

        // Extract header name (lowercasing happens in Headers.add)
        let name: String val = p.extract_string(p.pos, colon_pos)

        // Extract header value, skipping optional whitespace (OWS)
        var val_start = colon_pos + 1
        try
          while val_start < crlf do
            let ch = p.buf(val_start)?
            if (ch != ' ') and (ch != '\t') then break end
            val_start = val_start + 1
          end
        else
          _Unreachable()
        end

        // Trim trailing OWS from value
        var val_end = crlf
        try
          while val_end > val_start do
            let ch = p.buf(val_end - 1)?
            if (ch != ' ') and (ch != '\t') then break end
            val_end = val_end - 1
          end
        else
          _Unreachable()
        end

        let value: String val = p.extract_string(val_start, val_end)

        // Detect special headers
        let lower_name: String val = name.lower()
        if lower_name == "content-length" then
          match \exhaustive\ _parse_content_length(value)
          | let cl: USize =>
            match \exhaustive\ _content_length
            | let existing: USize =>
              if existing != cl then
                return InvalidContentLength
              end
            | None =>
              _content_length = cl
            end
          | InvalidContentLength => return InvalidContentLength
          end
        elseif lower_name == "transfer-encoding" then
          if value.lower().contains("chunked") then
            _chunked = true
          end
        end

        _headers.add(name, value)

        // Advance past this header line
        p.pos = crlf + 2
      | None =>
        // No complete line yet — check size limit
        let pending = p.buf.size() - p.pos
        if (pending + _total_header_bytes) > _config.max_header_size then
          return TooLarge
        end
        return _ParseNeedMore
        end
    end
    _Unreachable()
    _ParseNeedMore

  fun _parse_content_length(value: String val)
    : (USize | InvalidContentLength)
  =>
    """
    Parse a Content-Length value as a non-negative integer.
    """
    if value.size() == 0 then
      return InvalidContentLength
    end
    try
      var i: USize = 0
      while i < value.size() do
        let ch = value(i)?
        if (ch < '0') or (ch > '9') then
          return InvalidContentLength
        end
        i = i + 1
      end
      value.read_int[USize]()?._1
    else
      InvalidContentLength
    end

class _ExpectFixedBody is _ParserState
  """
  Reading a fixed-length response body (Content-Length).

  Delivers body data incrementally as it becomes available in the buffer.
  """
  var _remaining: USize

  new create(remaining: USize) =>
    _remaining = remaining

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    let available = (p.buf.size() - p.pos).min(_remaining)
    if available > 0 then
      let chunk: Array[U8] val =
        p.extract_bytes(p.pos, p.pos + available)
      p.handler.body_chunk(chunk)
      p.pos = p.pos + available
      _remaining = _remaining - available
    end

    if _remaining == 0 then
      p.handler.response_complete()
      p.state = _ExpectStatusLine(p.config)
      _ParseContinue
    else
      _ParseNeedMore
    end

class _ExpectChunkHeader is _ParserState
  """
  Expecting a chunk size line in chunked transfer encoding.

  Format: chunk-size [ chunk-ext ] CRLF
  """
  var _total_body_received: USize
  let _config: _ParserConfig

  new create(total_body_received: USize, config: _ParserConfig) =>
    _total_body_received = total_body_received
    _config = config

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    match \exhaustive\ _BufferScan.find_crlf(p.buf, p.pos)
    | let crlf: USize =>
      let line_len = crlf - p.pos
      if line_len > _config.max_chunk_header_size then
        return InvalidChunk
      end
      if line_len == 0 then
        return InvalidChunk
      end

      // Find optional chunk extension (semicolon)
      let size_end =
        match \exhaustive\ _BufferScan.find_byte(p.buf, ';', p.pos, crlf)
        | let i: USize => i
        | None => crlf
        end

      // Parse hex chunk size — must consume entire string
      let size_str: String val = p.extract_string(p.pos, size_end)
      let chunk_size =
        try
          (let cs, let consumed) = size_str.read_int[USize](0, 16)?
          if consumed.usize() != size_str.size() then
            return InvalidChunk
          end
          cs
        else
          return InvalidChunk
        end

      p.pos = crlf + 2

      if chunk_size == 0 then
        // Last chunk — expect trailers or final CRLF
        p.state = _ExpectChunkTrailer(0, _config)
        _ParseContinue
      else
        // Check body size limit
        if (_total_body_received + chunk_size) > _config.max_body_size then
          return BodyTooLarge
        end
        p.state =
          _ExpectChunkData(
            chunk_size, _total_body_received, _config)
        _ParseContinue
      end
    | None =>
      // No complete line — check size limit
      let pending = p.buf.size() - p.pos
      if pending > _config.max_chunk_header_size then
        InvalidChunk
      else
        _ParseNeedMore
      end
    end

class _ExpectChunkData is _ParserState
  """
  Reading chunk data in chunked transfer encoding.

  Delivers data incrementally, then expects CRLF after the chunk data.
  """
  var _remaining: USize
  var _total_body_received: USize
  let _config: _ParserConfig

  new create(
    remaining: USize,
    total_body_received: USize,
    config: _ParserConfig)
  =>
    _remaining = remaining
    _total_body_received = total_body_received
    _config = config

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    if _remaining > 0 then
      let available = (p.buf.size() - p.pos).min(_remaining)
      if available > 0 then
        let chunk: Array[U8] val =
          p.extract_bytes(p.pos, p.pos + available)
        p.handler.body_chunk(chunk)
        p.pos = p.pos + available
        _remaining = _remaining - available
        _total_body_received = _total_body_received + available
      end
      if _remaining > 0 then
        return _ParseNeedMore
      end
    end

    // Chunk data consumed — expect CRLF
    let bytes_available = p.buf.size() - p.pos
    if bytes_available < 2 then
      return _ParseNeedMore
    end

    try
      if (p.buf(p.pos)? == '\r') and (p.buf(p.pos + 1)? == '\n') then
        p.pos = p.pos + 2
        p.state = _ExpectChunkHeader(_total_body_received, _config)
        _ParseContinue
      else
        InvalidChunk
      end
    else
      _Unreachable()
      InvalidChunk
    end

class _ExpectChunkTrailer is _ParserState
  """
  Reading optional trailer headers after the last (zero-size) chunk.

  Trailers are skipped (not delivered to the receiver). The response is
  complete when an empty line is found.
  """
  var _total_trailer_bytes: USize
  let _config: _ParserConfig

  new create(total_trailer_bytes: USize, config: _ParserConfig) =>
    _total_trailer_bytes = total_trailer_bytes
    _config = config

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    while true do
      match \exhaustive\ _BufferScan.find_crlf(p.buf, p.pos)
      | let crlf: USize =>
        if crlf == p.pos then
          // Empty line: end of chunked message
          p.pos = crlf + 2
          p.handler.response_complete()
          p.state = _ExpectStatusLine(p.config)
          return _ParseContinue
        end

        // Skip this trailer header line
        let line_len = (crlf - p.pos) + 2
        _total_trailer_bytes = _total_trailer_bytes + line_len
        if _total_trailer_bytes > _config.max_header_size then
          return TooLarge
        end

        p.pos = crlf + 2
      | None =>
        let pending = p.buf.size() - p.pos
        if (pending + _total_trailer_bytes) > _config.max_header_size then
          return TooLarge
        end
        return _ParseNeedMore
      end
    end
    _Unreachable()
    _ParseNeedMore

class _ExpectCloseDelimitedBody is _ParserState
  """
  Reading a response body delimited by connection close.

  Used when the response has neither Content-Length nor Transfer-Encoding:
  chunked. Delivers all available data via `body_chunk()` and tracks total
  bytes received. Returns `_ParseNeedMore` to request more data. Completion
  is triggered externally by `_ResponseParser.connection_closed()`, not by
  `parse()`.
  """
  let _config: _ParserConfig
  var _total_received: USize = 0

  new create(config: _ParserConfig) =>
    _config = config

  fun ref parse(p: _ResponseParser ref): _ParseResult =>
    let available = p.buf.size() - p.pos
    if available > 0 then
      let chunk: Array[U8] val =
        p.extract_bytes(p.pos, p.pos + available)
      p.pos = p.pos + available
      _total_received = _total_received + available

      if _total_received > _config.max_body_size then
        return BodyTooLarge
      end

      p.handler.body_chunk(chunk)
    end
    _ParseNeedMore
