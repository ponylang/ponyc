use "buffered"
use "net"

primitive _PayloadRequest
primitive _PayloadResponse
primitive _PayloadHeaders
primitive _PayloadContentLength
primitive _PayloadChunkStart
primitive _PayloadChunk
primitive _PayloadChunkEnd
primitive _PayloadBody
primitive _PayloadReady
primitive _PayloadError

type _PayloadState is
  ( _PayloadRequest
  | _PayloadResponse
  | _PayloadHeaders
  | _PayloadContentLength
  | _PayloadChunkStart
  | _PayloadChunk
  | _PayloadChunkEnd
  | _PayloadBody
  | _PayloadReady
  | _PayloadError
  )

class _PayloadBuilder
  """
  This builds a Payload using received chunks of data.
  """
  let _client: Bool
  var _state: _PayloadState
  var _payload: Payload
  var _content_length: USize = 0
  var _chunked: Bool = false

  new request() =>
    """
    Expect HTTP requests.
    """
    _client = false
    _state = _PayloadRequest
    _payload = Payload.request()

  new response() =>
    """
    Expect HTTP responses.
    """
    _client = true
    _state = _PayloadResponse
    _payload = Payload.response()

  fun state(): _PayloadState =>
    """
    Returns the parse state.
    """
    _state

  fun ref parse(buffer: Reader) =>
    """
    Parse available data based on our state. _ResponseBody is not listed here.
    In that state, we wait for the connection to close and treat all pending
    data as the response body.
    """
    match _state
    | _PayloadRequest => _parse_request(buffer)
    | _PayloadResponse => _parse_response(buffer)
    | _PayloadHeaders => _parse_headers(buffer)
    | _PayloadChunkStart => _parse_chunk_start(buffer)
    | _PayloadChunk => _parse_chunk(buffer)
    | _PayloadChunkEnd => _parse_chunk_end(buffer)
    | _PayloadContentLength => _parse_content_length(buffer)
    end

  fun ref done(): Payload^ =>
    """
    Finish parsing. Returns the payload if it is ready, otherwise an empty
    payload.
    """
    let result = _state = if _client then
      _PayloadResponse
    else
      _PayloadRequest
    end

    var payload = _payload = Payload._empty(_client)
    _content_length = 0
    _chunked = false

    if result isnt _PayloadReady then
      payload = Payload._empty(_client)
    end

    payload

  fun ref closed(buffer: Reader) =>
    """
    The connection has closed, which may signal that all remaining data is the
    payload body.
    """
    if _state is _PayloadBody then
      _content_length = buffer.size()

      try
        let chunk = buffer.block(_content_length)
        _payload.add_chunk(consume chunk)
        _state = _PayloadReady
      end
    end

  fun ref _parse_request(buffer: Reader) =>
    """
    Look for: "<Method> <URL> <Proto>".
    """
    try
      let line = buffer.line()

      try
        let method_end = line.find(" ")
        _payload.method = line.substring(0, method_end)

        let url_end = line.find(" ", method_end + 1)
        _payload.url = URL.valid(line.substring(method_end + 1, url_end))
        _payload.proto = line.substring(url_end + 1)

        _state = _PayloadHeaders
        parse(buffer)
      else
        _state = _PayloadError
      end
    end

  fun ref _parse_response(buffer: Reader) =>
    """
    Look for: "<Proto> <Code> <Description>".
    """
    try
      let line = buffer.line()

      try
        let proto_end = line.find(" ")
        _payload.proto = line.substring(0, proto_end)
        _payload.status = line.read_int[U16](proto_end + 1)._1

        let status_end = line.find(" ", proto_end + 1)
        _payload.method = line.substring(status_end + 1)

        _state = _PayloadHeaders
        parse(buffer)
      else
        _state = _PayloadError
      end
    end

  fun ref _parse_headers(buffer: Reader) =>
    """
    Look for: "<Key>:<Value>" or an empty line.
    """
    try
      while true do
        let line = buffer.line()

        if line.size() > 0 then
          try
            let i = line.find(":")
            let key = recover val line.substring(0, i).>strip() end
            let value = recover val line.substring(i + 1).>strip() end
            _payload(key) = value

            match key.lower()
            | "content-length" =>
              _content_length = value.read_int[USize]()._1
            | "transfer-encoding" =>
              try
                value.find("chunked")
                _chunked = true
              end
            | "host" =>
              // TODO: set url host and service
              None
            | "authorization" =>
              // TODO: set url username and password
              None
            end
          else
            _state = _PayloadError
          end
        else
          if
            (_payload.status == 204) or
            (_payload.status == 304) or
            ((_payload.status > 0) and (_payload.status < 200))
          then
            _state = _PayloadReady
          elseif _chunked then
            _content_length = 0
            _state = _PayloadChunkStart
            parse(buffer)
          elseif _content_length > 0 then
            _state = _PayloadContentLength
            parse(buffer)
          else
            if _client then
              _state = _PayloadBody
              parse(buffer)
            else
              _state = _PayloadReady
            end
          end
          return
        end
      end
    end

  fun ref _parse_content_length(buffer: Reader) =>
    """
    Look for _content_length available bytes.
    """
    try
      let body = buffer.block(_content_length)
      _payload.add_chunk(consume body)
      _state = _PayloadReady
    end

  fun ref _parse_chunk_start(buffer: Reader) =>
    """
    Look for the beginning of a chunk.
    """
    try
      let line = buffer.line()

      if line.size() > 0 then
        _content_length = line.read_int[USize](0, 16)._1

        if _content_length > 0 then
          _state = _PayloadChunk
        else
          _state = _PayloadChunkEnd
        end

        parse(buffer)
      else
        _content_length = 0
        _state = _PayloadError
      end
    end

  fun ref _parse_chunk(buffer: Reader) =>
    """
    Look for a chunk.
    """
    try
      let chunk = buffer.block(_content_length)
      _payload.add_chunk(consume chunk)
      _state = _PayloadChunkEnd
      parse(buffer)
    end

  fun ref _parse_chunk_end(buffer: Reader) =>
    """
    Look for a blank line.
    """
    try
      let line = buffer.line()

      if line.size() == 0 then
        if _content_length > 0 then
          _state = _PayloadChunkStart
          parse(buffer)
        else
          _state = _PayloadReady
        end
      else
        _state = _PayloadError
      end
    end
