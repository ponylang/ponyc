use "net"
use "net/ssl"

class _ServerListener is TCPListenNotify
  """
  Manages the listening socket for an HTTP server.
  """
  let _server: Server
  let _sslctx: (SSLContext | None)
  let _routes: Routes val

  new iso create(server: Server, sslctx: (SSLContext | None),
    routes: Routes val)
  =>
    """
    Creates a new listening socket manager.
    """
    _server = server
    _sslctx = sslctx
    _routes = routes

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

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ ? =>
    """
    Create a notifier for a specific HTTP socket.
    """
    // TODO:
    error
