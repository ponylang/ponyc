use "collections"
use "net"
use "net/ssl"

actor _ClientConnection
  """
  Manages a persistent and possibly pipelined TCP connection to an HTTP server.
  """
  let _auth: TCPConnectionAuth
  let _host: String
  let _service: String
  let _sslctx: (SSLContext | None)
  let _pipeline: Bool
  let _unsent: List[Payload val] = _unsent.create()
  let _sent: List[Payload val] = _sent.create()
  var _conn: (TCPConnection | None) = None

  new create(auth: TCPConnectionAuth, host: String, service: String,
    sslctx: (SSLContext | None) = None, pipeline: Bool = true)
  =>
    """
    Create a connection for the given host and service.
    """
    _auth = auth
    _host = host
    _service = service
    _sslctx = sslctx
    _pipeline = pipeline

  be apply(request: Payload val) =>
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
          node.pop()._client_fail()
          return
        end
      end

      for node in _sent.nodes() do
        if node() is request then
          node.pop()._client_fail()
          break
        end
      end
    end

  be _response(response: Payload) =>
    """
    Call the request's handler and supply the response.
    """
    try _sent.shift()._client_respond(consume response) end
    _send()

  be _connected(conn: TCPConnection) =>
    """
    The connection to the server has been established. Send pending requests.
    """
    _send()

  be _connect_failed(conn: TCPConnection) =>
    """
    The connection couldn't be established. Cancel all pending requests.
    """
    _cancel_all()
    _conn = None

  be _auth_failed(conn: TCPConnection) =>
    """
    The connection couldn't be authenticated. Cancel all pending requests.
    """
    _cancel_all()
    _conn = None

  be _closed(conn: TCPConnection) =>
    """
    The connection to the server has closed prematurely. Cancel everything.
    """
    if conn is _conn then
      _cancel_all()
      _conn = None
    end

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
    _conn = try
      let ctx = _sslctx as SSLContext
      let ssl = ctx.client(_host)
      TCPConnection(_auth, SSLConnection(_ResponseBuilder(this), consume ssl),
        _host, _service)
    else
      TCPConnection(_auth, _ResponseBuilder(this), _host, _service)
    end

  fun ref _cancel_all() =>
    """
    Cancel all pending requests.
    """
    try
      while true do
        _unsent.pop()._client_fail()
      end
    end

    for node in _sent.nodes() do
      node.remove()
      try node.pop()._client_fail() end
    end
