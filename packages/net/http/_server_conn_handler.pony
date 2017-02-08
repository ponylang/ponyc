use "buffered"
use "net"

class _ServerConnHandler is TCPConnectionNotify
  """
  This is the network notification handler for the server.  It uses
  `PayloadBuilder` to assemble request `Payload` objects using received
  chunks of data.  Functions in this class execute within the
  `TCPConnection` actor.
  """
  let _handlermaker: HandlerFactory val
  let _logger: Logger
  let _reversedns: (DNSLookupAuth | None)
  let _buffer: Reader = Reader
  var _parser: (_HTTPParser | None) = None
  var _session: (_ServerConnection | None) = None
  let _registry: HTTPServer tag
  
  new iso create(handlermaker: HandlerFactory val, logger: Logger,
    reversedns: (DNSLookupAuth | None), registry: HTTPServer)
    =>
    """
    Initialize the context for parsing incoming HTTP requests.
    """
    _logger = logger
    _reversedns = reversedns
    _handlermaker = handlermaker
    _registry = registry

  fun ref accepted(conn: TCPConnection ref) =>
    """
    Accept the incoming TCP connection and create the actor that will
    manage further communication, and the message parser that feeds it.
    """
    (let host, let port) = try
      conn.remote_address().name(_reversedns)
    else
      ("-", "-")
    end

    _registry.register_session(conn)
    _session = _ServerConnection(_handlermaker, _logger, conn, host)
    try
      _parser = _HTTPParser.request(_session as _ServerConnection)
    end

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso): Bool =>
    """
    Pass chunks of data to the `HTTPParser` for this session.  It will
    then pass completed information on the the `HTTPSession`.
    """
    // TODO: inactivity timer
    // add a "reset" API to Timers
    _buffer.append(consume data)

    match _parser
    | let b: _HTTPParser =>
        try
          // Let the parser take a look at what has been received.
          b.parse(_buffer)
        else
          // Any syntax errors will terminate the connection.
          conn.close()
        end
      end
    true

  fun ref throttled(conn: TCPConnection ref) =>
    """
    Notification that the TCP connection to the client can not accept data
    for a while.
    """
    try
      (_session as _ServerConnection).throttled()
    end

  fun ref unthrottled( conn: TCPConnection ref) =>
    """
    Notification that the TCP connection can resume accepting data.
    """
    try
      (_session as  _ServerConnection).unthrottled()
    end

  fun ref closed(conn: TCPConnection ref) =>
    """
    The connection has been closed.  Abort the session.
    """
    _registry.unregister_session(conn)
    try
      (_session as _ServerConnection).cancel(Payload.request())
    end
