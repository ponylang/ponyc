use "collections"
use "net"
use "net/ssl"

actor Client
  """
  Manages a persistent and possibly pipelined TCP connection to an HTTP server.
  """
  let _host: String
  let _service: String
  let _sslctx: (SSLContext | None)
  let _pipeline: Bool
  let _unsent: List[Payload] = _unsent.create()
  let _sent: List[Payload] = _sent.create()
  var _conn: (TCPConnection | None) = None

  new create(host: String, service: String, sslctx: (SSLContext | None) = None,
    pipeline: Bool = true)
  =>
    """
    Create a client for the given host and service.
    """
    _host = host
    _service = service
    _sslctx = sslctx
    _pipeline = pipeline

  be apply(request: Payload) =>
    """
    Schedule a request.
    """
    _unsent.push(consume request)
    _send()

  be cancel(request: Payload tag) =>
    """
    Cancel a request.
    """
    try
      for node in _unsent.nodes() do
        if node() is request then
          node.remove()
          node.pop()._handle(Payload)
          return
        end
      end

      for node in _sent.nodes() do
        if node() is request then
          (node.pop() as Payload^)._handle(Payload)
          break
        end
      end
    end

  be _response(response: Payload) =>
    """
    Call the request's handler and supply the response.
    """
    try
      (_sent.shift() as Payload^)._handle(consume response)
      _send()
    end

  be _connected() =>
    """
    The connection to the server has been established. Send pending requests.
    """
    _send()

  be _connect_failed() =>
    """
    The connection couldn't be established. Cancel all pending requests.
    """
    _cancel_all()
    _conn = None

  be _auth_failed() =>
    """
    The connection couldn't be authenticated. Cancel all pending requests.
    """
    _cancel_all()
    _conn = None

  be _closed() =>
    """
    The connection to the server has closed prematurely. Reschedule sent
    requests that haven't been cancelled.
    """
    try
      for node in _sent.rnodes() do
        node.remove()

        try
          _unsent.unshift(node.pop() as Payload^)
        end
      end
    end

    _conn = None
    _send()

  fun ref _send() =>
    """
    Send pending requests. If the connection is closed, open it. If we have
    nothing to send and we aren't waiting on any responses, close the
    connection.
    """
    if _unsent.size() == 0 then
      if _sent.size() == 0 then
        try
          (_conn as TCPConnection).dispose()
          _conn = None
        end
      end
      return
    end

    if (_sent.size() > 0) and not _pipeline then
      return
    end

    try
      let conn = _conn as TCPConnection

      try
        repeat
          let request = _unsent.shift()
          request._write(conn)
          _sent.push(consume request)
        until not _pipeline end
      end
    else
      _new_conn()
    end

  fun ref _new_conn() =>
    """
    Creates a new connection.
    """
    match _sslctx
    | var ctx: SSLContext =>
      try
        let ssl = ctx.client(_host)
        _conn = TCPConnection(
          SSLConnection(_ResponseBuilder(this), consume ssl), _host, _service)
        return
      end
    end

    _conn = TCPConnection(_ResponseBuilder(this), _host, _service)

  fun ref _cancel_all() =>
    """
    Cancel all pending requests.
    """
    try
      while true do
        _unsent.pop()._handle(Payload)
      end
    end

    try
      while true do
        (_sent.pop() as Payload^)._handle(Payload)
      end
    end
