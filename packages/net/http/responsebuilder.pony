use "net"

primitive _ResponseStatus
primitive _ResponseHeaders
primitive _ResponseContentLength
primitive _ResponseChunkStart
primitive _ResponseChunk
primitive _ResponseChunkEnd
primitive _ResponseBody
primitive _ResponseReady
primitive _ResponseError

type _ResponseState is
  ( _ResponseStatus
  | _ResponseHeaders
  | _ResponseContentLength
  | _ResponseChunkStart
  | _ResponseChunk
  | _ResponseChunkEnd
  | _ResponseBody
  | _ResponseReady
  | _ResponseError
  )

class _ResponseBuilder is TCPConnectionNotify
  """
  This builds a Response using received chunks of data.
  """
  let _client: Client
  let _buffer: Buffer = Buffer
  var _response: Response iso = Response
  var _state: _ResponseState = _ResponseStatus
  var _content_length: U64 = 0
  var _chunked: Bool = false

  new iso create(client: Client) =>
    """
    The response builder needs to know which client to forward the response to.
    """
    _client = client

  fun ref connected(conn: TCPConnection ref) =>
    """
    Tell the client we have connected.
    """
    _client._connected()

  fun ref connect_failed(conn: TCPConnection ref) =>
    """
    The connection could not be established. Tell the client not to proceed.
    """
    _client._connect_failed()

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Assemble chunks of data into a response. When we have a whole response,
    give it to the client and start a new one.
    """
    _buffer.append(consume data)
    _parse()

  fun ref closed(conn: TCPConnection ref) =>
    """
    The connection has closed, possibly prematurely.
    """
    if _state is _ResponseBody then
      _content_length = _buffer.size()

      try
        let chunk = _buffer.block(_content_length)
        _response.add_chunk(consume chunk)
        _state = _ResponseReady
        _parse()
      end
    end

    _close()

  fun ref _parse() =>
    """
    Parse available data based on our state. _ResponseBody is not listed here.
    In that state, we wait for the connection to close and treat all pending
    data as the response body.
    """
    match _state
    | _ResponseStatus => _parse_status()
    | _ResponseHeaders => _parse_headers()
    | _ResponseChunkStart => _parse_chunk_start()
    | _ResponseChunk => _parse_chunk()
    | _ResponseChunkEnd => _parse_chunk_end()
    | _ResponseContentLength => _parse_content_length()
    | _ResponseReady =>
      _client._response(_response = Response)
      _state = _ResponseStatus
    | _ResponseError =>
      _response = Response
      _client._response(Response)
      _state = _ResponseStatus
    end

  fun ref _close() =>
    """
    Tell the client we have disconnected.
    """
    _response = Response
    _state = _ResponseStatus
    _content_length = 0
    _chunked = false

    _buffer.clear()
    _client._closed()

  fun ref _parse_status() =>
    """
    Look for: "HTTP/1.1 <Code> <Description>".
    """
    try
      let line = _buffer.line()

      try
        if line.at("HTTP/1.1 ", 0) then
          _response.set_proto("HTTP/1.1")
        elseif line.at("HTTP/1.0 ", 0) then
          _response.set_proto("HTTP/1.0")
        else
          error
        end

        _response.set_status(line.u16(9))

        let offset = line.find(" ", 9)
        _response.set_status_text(line.substring(offset + 1, -1))
        _state = _ResponseHeaders
        _parse()
      else
        _close()
      end
    end

  fun ref _parse_headers() =>
    """
    Look for: "<Key>:<Value>" or an empty line.
    """
    try
      while true do
        let line = _buffer.line()

        if line.size() > 0 then
          try
            let i = line.find(":")
            let key = recover val line.substring(0, i - 1).trim() end
            let value = recover val line.substring(i + 1, -1).trim() end
            _response(key) = value

            match key
            | "Content-Length" =>
              _content_length = value.u64()
            | "Transfer-Encoding" =>
              try
                value.find("chunked")
                _chunked = true
              end
            end
          else
            _close()
          end
        else
          if
            (_response.status() == 204) or
            (_response.status() == 304) or
            ((_response.status() / 100) == 1)
          then
            _state = _ResponseReady
          elseif _chunked then
            _content_length = 0
            _state = _ResponseChunkStart
            _parse()
          elseif _content_length > 0 then
            _state = _ResponseContentLength
            _parse()
          else
            _state = _ResponseBody
            _parse()
          end
          return
        end
      end
    end

  fun ref _parse_content_length() =>
    """
    Look for _content_length available bytes.
    """
    try
      let body = _buffer.block(_content_length)
      _response.add_chunk(consume body)
      _state = _ResponseReady
      _parse()
    end

  fun ref _parse_chunk_start() =>
    """
    Look for the beginning of a chunk.
    """
    try
      let line = _buffer.line()

      if line.size() > 0 then
        _content_length = line.u64(0, 16)

        if _content_length > 0 then
          _state = _ResponseChunk
        else
          _state = _ResponseChunkEnd
        end
      else
        _content_length = 0
        _state = _ResponseError
      end

      _parse()
    end

  fun ref _parse_chunk() =>
    """
    Look for a chunk.
    """
    try
      let chunk = _buffer.block(_content_length)
      _response.add_chunk(consume chunk)
      _state = _ResponseChunkEnd
      _parse()
    end

  fun ref _parse_chunk_end() =>
    """
    Look for a blank line.
    """
    try
      let line = _buffer.line()

      if line.size() == 0 then
        if _content_length > 0 then
          _state = _ResponseChunkStart
        else
          _state = _ResponseReady
        end
      else
        _state = _ResponseError
      end

      _parse()
    end
