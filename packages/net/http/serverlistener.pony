use "net"
use "net/ssl"

class _ServerListener is TCPListenNotify
  """
  Manages the listening socket for an HTTP server.
  """
  let _server: Server
  let _sslctx: (SSLContext | None)
  let _handler: RequestHandler
  let _logger: Logger

  new iso create(server: Server, sslctx: (SSLContext | None),
    handler: RequestHandler, logger: Logger)
  =>
    """
    Creates a new listening socket manager.
    """
    _server = server
    _sslctx = sslctx
    _handler = handler
    _logger = logger

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
    Create a notifier for a specific HTTP socket.
    """
    try
      let ctx = _sslctx as SSLContext
      let ssl = ctx.server()
      SSLConnection(_RequestBuilder(_handler, _logger), consume ssl)
    else
      _RequestBuilder(_handler, _logger)
    end
