use "net"
use "net/ssl"

class _ServerListener is TCPListenNotify
  """
  Manages the listening socket for an HTTP server.  Incoming requests
  are assembled and dispatched.
  """
  let _server: HTTPServer
  let _sslctx: (SSLContext | None)
  let _handlermaker: HandlerFactory val
  let _logger: Logger
  let _reversedns: (DNSLookupAuth | None)

  new iso create(server: HTTPServer, sslctx: (SSLContext | None),
    handler: HandlerFactory val,  // Makes a unique session handler
    logger: Logger, reversedns: (DNSLookupAuth | None))
  =>
    """
    Creates a new listening socket manager.
    """
    _server = server
    _sslctx = sslctx
    _handlermaker = handler
    _logger = logger
    _reversedns = reversedns

  fun ref listening(listen: TCPListener ref) =>
    """
    Inform the server of the bound IP address.
    """
    _server._listening(listen.local_address())

  fun ref not_listening(listen: TCPListener ref) =>
    """
    Inform the server we failed to listen.
    """
    _server._not_listening()

  fun ref closed(listen: TCPListener ref) =>
    """
    Inform the server we have stopped listening.
    """
    _server._closed()

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    """
    Create a notifier for a specific HTTP socket.  A new instance of the
    back-end Handler is passed along so it can be used on each `Payload`.
    """
    try
      let ctx = _sslctx as SSLContext
      let ssl = ctx.server()
      SSLConnection(
        _ServerConnHandler(_handlermaker, _logger, _reversedns, _server),
        consume ssl)
    else
      _ServerConnHandler(_handlermaker, _logger, _reversedns, _server)
    end
