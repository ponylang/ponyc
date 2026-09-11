class ref ResponseCollector
  """
  Accumulates streaming response callbacks into a buffered `HTTPResponse`.

  Create a fresh `ResponseCollector` for each request/response cycle. On
  keep-alive connections where multiple requests are sent sequentially, use a
  new collector per request rather than reusing one.

  Typical usage in `HTTPClientLifecycleEventReceiver` callbacks:

  ```pony
  var _collector: ResponseCollector = ResponseCollector

  fun ref on_response(response: Response val) =>
    _collector = ResponseCollector
    _collector.set_response(response)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _collector.add_chunk(data)

  fun ref on_response_complete() =>
    try
      let response = _collector.build()?
      // use response.status, response.body, etc.
    end
  ```

  The collector concatenates all chunks into a single contiguous `Array[U8]
  val`. For large responses, this means the full body is held in memory. Users
  who need streaming for large downloads should use the raw `on_body_chunk()`
  callbacks directly.
  """
  var _response: (Response val | None)
  embed _chunks: Array[Array[U8] val]
  var _total_size: USize

  new create() =>
    """
    Create an empty response collector.
    """
    _response = None
    _chunks = Array[Array[U8] val]
    _total_size = 0

  fun ref set_response(response: Response val) =>
    """
    Store the response metadata (version, status, reason, headers).

    Must be called before `build()`. Typically called from `on_response()`.
    """
    _response = response

  fun ref add_chunk(data: Array[U8] val) =>
    """
    Append a body chunk. Typically called from `on_body_chunk()`.

    Chunks are concatenated in order when `build()` is called.
    """
    _total_size = _total_size + data.size()
    _chunks.push(data)

  fun build(): HTTPResponse val ? =>
    """
    Build the final `HTTPResponse` from the stored response and chunks.

    Returns the buffered response with all chunks concatenated into a single
    `Array[U8] val`.

    Partial: errors if `set_response()` was never called, since the collector
    would have no status, version, or headers to populate.
    """
    let response = _response as Response val
    var body: Array[U8] iso = recover iso Array[U8](_total_size) end
    for chunk in _chunks.values() do
      body.append(chunk)
    end
    HTTPResponse(
      response.version,
      response.status,
      response.reason,
      response.headers,
      consume body)
