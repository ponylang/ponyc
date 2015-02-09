use "net"

primitive _ResponseStatus
primitive _ResponseHeaders
primitive _ResponseBody
primitive _ResponseReady

type _ResponseState is
  ( _ResponseStatus
  | _ResponseHeaders
  | _ResponseBody
  | _ResponseReady
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
    The connection could not be established. Treat this the same way as a parse
    failure or a premature disconnect.
    """
    _close()

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
    _close()

  fun ref _parse() =>
    """
    Parse available data based on our state.
    """
    match _state
    | _ResponseStatus => _parse_status()
    | _ResponseHeaders => _parse_headers()
    | _ResponseBody => _parse_body()
    end

    if _state is _ResponseReady then
      _client._response(_response = Response)
    end

  fun ref _close() =>
    """
    Tell the client we have disconnected.
    """
    _response = Response
    _state = _ResponseStatus
    _content_length = 0

    _buffer.clear()
    _client._closed()

  fun ref _parse_status() =>
    """
    Look for: "HTTP/1.1 <Code> <Description>".
    """
    try
      let line = _buffer.line()

      try
        Fact(line.at("HTTP/1.1 ", 0))
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

            if key == "Content-Length" then
              _content_length = value.u64()
            end
          else
            _close()
          end
        else
          _state = if _content_length > 0 then
            _ResponseBody
          else
            _ResponseReady
          end
          return
        end
      end
    end

  fun ref _parse_body() =>
    """
    Look for _content_length available bytes.
    """
    try
      let body = _buffer.block(_content_length)
      _response.set_body(consume body)
      _state = _ResponseReady
    end
