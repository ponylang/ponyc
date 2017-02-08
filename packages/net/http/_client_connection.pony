use "collections"
use "net"
use "net/ssl"

actor _ClientConnection is HTTPSession
  """
  Manages a persistent and possibly pipelined TCP connection to an HTTP server.

  This is where pipelining happens, if it has been enabled by the `Client`.
  Only "safe" requests (GET, HEAD, OPTIONS) are sent to the server if
  *any* responses are still pending.

  The `HTTPHandler.send_body` notification function will be called if/when
  the `HTTPSession` is ready to receive body data for `POST` operations in
  transfer modes other than `Oneshot`.

  ### Receiving Responses

  Reception is handled through an `HTTPHandler` instance.
  `HTTPHandler.apply` signals the arrival of a message with headers.
  How the body data is obtained depends on the `transfer` mode.

  * For `StreamTranfer` and `ChunkedTransfer` modes, there will be
  any number of `HTTPHandler.chunk` notifications, followed by
  an `HTTPHandler.finished` notification.

  * For `OneShotTransfer` mode, the client application just needs to
  call `Payload.body` once to get the entire body.

  ## The HandlerFactory

  The `Client` class will try to re-use sessions.  If it needs to create
  a new session, based on the request URL, it will do that, and then it
  will need a new instance of the caller's `HTTPHandler` class.
  Since the client application code does not know in advance when this
   will be necessary, it passes in a `HandlerFactory` that creates the
  actual `HTTPHandler`, customized
  for the client application's needs.
  """
  let _auth: TCPConnectionAuth
  let _host: String
  let _service: String
  let _sslctx: (SSLContext | None)
  let _pipeline: Bool
  let _app_handler: HTTPHandler
  let _unsent: List[Payload val] = _unsent.create()
  let _sent: List[Payload val] = _sent.create()
  var _safewait: Bool = false
  var _conn: (TCPConnection | None) = None
  var _nobackpressure: Bool = true   // TCP backpressure indicator
  
  new create(auth: TCPConnectionAuth, host: String, service: String,
    sslctx: (SSLContext | None) = None, pipeline: Bool = true,
    handlermaker: HandlerFactory val)
    =>
    """
    Create a connection for the given host and service.  We also create
    an instance of the client application's HTTPHandler.
    """
    _auth = auth
    _host = host
    _service = service
    _sslctx = sslctx
    _pipeline = pipeline
    _app_handler = handlermaker( this )

  be apply(request: Payload val) =>
    """
    Schedule a request to be sent by adding it to the `unsent` queue
    for this session.
    """
    _unsent.push(consume request)
    _send_pending()

  be cancel(request: Payload val) =>
    """
    Cancel a request.
    """
    // We look for it first in the unsent queue.  If it is there,
    // we just remove it.
    try
      for node in _unsent.nodes() do
        if node() is request then
          node.remove()
          node.pop()
          return
        end
      end

      // It might have been sent already, but no response received
      // yet.  In that case we have to close the connection so that
      // the server finds out.
      for node in _sent.nodes() do
        if node() is request then
          try (_conn as TCPConnection).dispose() end
          _conn = None
          node.pop()
          break
        end
      end
    end

  be _deliver(response: Payload val) =>
    """
    Deal with a new Response coming back from the server.

    Since the session operates in a FIFO manner, the Request corresponding
    to this Response is the oldest one on the `_sent` list.  We take it
    off that list and call its handler.  It becomes the 'currently being
    delivered' response and subsequent body data has to go there as well,
    if there is any.
    """
    try
      let request = _sent.shift()
      _app_handler( response )

      // If that request has no body data coming, we can go look
      // for more requests to send.
      if response.transfer_mode is OneshotTransfer then
        _send_pending()
      end
    end

  be _connected(conn: TCPConnection) =>
    """
    The connection to the server has been established. Send pending requests.
    """
    _nobackpressure = true
    _send_pending()

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

  be write( data: ByteSeq val ) =>
    """
    Write a low-level byte stream.  The `Payload` objects call this to
    generate their wire representation.
    """
    match _conn
    | let c: TCPConnection => c.write( data )
    end

  be _chunk( data: ByteSeq val ) =>
    """
    Called when *inbound* body data has arrived for the currently
    inbound `Payload`.  This should be passed directly to the application's
    `HTTPHandler.chunk` method.
    """
    _app_handler.chunk( data )

  be _finish() =>
    """
    Inidcates that the last *inbound* body chunk has been sent to
    `_chunk`.  This is passed on to the front end.
    """
    _app_handler.finished()

  be finish() =>
    """
    We are done sending a request with a long body.
    """
    None

  be throttled() =>
    """
    The connection to the server can not accept data for a while.
    We set a local flag too so we do not send anything on the queue.
    """
    _nobackpressure = false
    _app_handler.throttled()

  be unthrottled() =>
    """
    The connection to the server can now accept more data.
    """
    _nobackpressure = true
    _app_handler.unthrottled()
    _send_pending()

  fun ref _send_pending() =>
    """
    Send pending requests to the server. If the connection is closed,
    open it. If we have nothing to send and we aren't waiting on any
    responses, close the connection.
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

    // If waiting for response to an unsafe request, do not send more requests.
    // TODO this check has to be in Client so that the apply fails.
    if _safewait then return end

    try
      // Get the existing connection, if it is there.
      let conn = _conn as TCPConnection

      try
        // Send requests until backpressure makes us stop, or we
        // send an unsafe request.
        while _nobackpressure do
          // Take a request off the unsent queue and notice whether
          // it is safe.
          let request = _unsent.shift()
          let safereq = request.is_safe()
          // Send all of the request that is possible for now.
          request._write( true, conn )

          // If there is a folow-on body, tell client to send it now.
          if request.has_body() then
            match request.transfer_mode
            | OneshotTransfer => finish()
            else
              _app_handler.need_body()
            end
          else
            finish()
          end

          // Put the request on the list of things we are waiting for.
          _sent.push(consume request)

          // If it was not 'safe', send no more for now.
          if not safereq then
            _safewait = true
            break
          end
        end
      end
    else
      // Oops, the connection is closed.  Open it and try sending
      // again when it becomes active.
      _new_conn()
    end

  fun ref _new_conn() =>
    """
    Creates a new connection.  `ResponseBuilder` is the notification class
    that will send back a `_connected` call when the connection has been made.
    """
    _conn = try
      let ctx = _sslctx as SSLContext
      let ssl = ctx.client(_host)
      TCPConnection(_auth,
                    SSLConnection(_ClientConnHandler(this),
                                  consume ssl),
                    _host, _service)
    else
      TCPConnection(_auth,
                    _ClientConnHandler(this),
                    _host, _service)
    end

  fun ref _cancel_all() =>
    """
    Cancel all pending requests.
    """
    try
      while true do
        _unsent.pop() //TODO send fail response
      end
    end

    for node in _sent.nodes() do
      node.remove()
      try node.pop() end //TODO send fail response
    end

  be _mute() =>
    """
    The application can not handle any more data for a while.
    """
    try (_conn as TCPConnection).mute() end

  be _unmute() =>
    """
    The application can accept more data.
    """
    try (_conn as TCPConnection).unmute() end
