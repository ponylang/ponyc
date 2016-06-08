use "collections"
use "net/tcp"

actor _ServerConnection
  """
  Manages a stream of requests to a server, ordering the responses.
  """
  let _handler: RequestHandler
  let _logger: Logger
  let _conn: TCPConnection
  let _client_ip: String
  let _pending: List[Payload] = _pending.create()
  let _dispatched: List[Payload tag] = _dispatched.create()
  let _responses: MapIs[Payload tag, (Payload val, Payload val)] =
    _responses.create()
  var _safe: Bool = true

  new create(handler: RequestHandler, logger: Logger, conn: TCPConnection,
    client_ip: String)
  =>
    """
    The server connection needs to know how to handle requests and the
    connection to write responses on.
    """
    _handler = handler
    _logger = logger
    _conn = conn
    _client_ip = client_ip

  be dispatch(request: Payload) =>
    """
    Dispatch requests. We can have many safe requests dispatched at once, or a
    single unsafe request, since the client may expect an initial unsafe
    request to alter the answer to following requests.
    """
    request.handler = recover this~answer() end
    let safe = is_safe(request.method)

    if (_safe and safe) or (_dispatched.size() == 0) then
      _dispatched.push(request)
      _handler(consume request)
      _safe = safe
    else
      _pending.push(consume request)
    end

  be answer(request: Payload val, response: Payload val) =>
    """
    Associate a response with the given request and send responses in the
    correct order.
    """
    if response.status == 0 then
      // Shutdown the connection on an error response.
      _conn.dispose()
      _pending.clear()
      _dispatched.clear()
      _responses.clear()
      return
    end

    try
      if _dispatched() is request then
        // If this is next request, write the response and check for any stored
        // responses that can now be written in the correct order.
        _send(request, response)
        _send_responses()
      else
        // Store the response to be written later.
        _responses(request) = (request, response)
      end
    end

  fun is_safe(method: String): Bool =>
    """
    Return true for a safe request method, false otherwise.
    """
    match method
    | "GET"
    | "HEAD"
    | "OPTIONS" =>
      true
    else
      false
    end

  fun ref _dispatch_pending() =>
    """
    If we have pending requests, dispatch as many as we can.
    """
    try
      while _pending.size() > 0 do
        let safe = is_safe(_pending().method)

        if (_safe and safe) or (_dispatched.size() == 0) then
          let request = _pending.pop()
          _dispatched.push(request)
          _handler(consume request)
          _safe = safe
        end
      end
    end

  fun ref _send_responses() =>
    """
    Send available responses in the correct order.
    """
    try
      while _dispatched.size() > 0 do
        (let request, let response) = _responses(_dispatched())
        _send(request, response)
      end
    end

  fun ref _send(request: Payload val, response: Payload val) =>
    """
    Send a single response.
    """
    try
      let keepalive = try
        request("Connection") != "close"
      else
        request.proto != "HTTP/1.0"
      end

      _dispatched.shift()
      response._write(_conn, keepalive)

      if not keepalive then
        _conn.dispose()
        _pending.clear()
        _dispatched.clear()
        _responses.clear()
      end

      _logger(_client_ip, request, response)
    end
