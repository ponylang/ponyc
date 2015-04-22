use "collections"
use "net"
use "net/ssl"

actor Server
  """
  Runs an HTTP server. When routes are changed, the changes are only reflected
  for new connections. Existing connections continue to use the old routes.
  """
  let _notify: ServerNotify
  let _sslctx: (SSLContext | None)
  let _listen: TCPListener
  var _address: IPAddress
  var _dirty_routes: Bool = false

  new create(notify: ServerNotify iso, handler: RequestHandler,
    host: String = "", service: String = "0",
    sslctx: (SSLContext | None) = None)
  =>
    """
    Create a server bound to the given host and service.
    """
    _notify = consume notify
    _sslctx = sslctx
    _listen = TCPListener(_ServerListener(this, sslctx, handler),
      host, service)
    _address = recover IPAddress end

  be set_handler(handler: RequestHandler) =>
    """
    Replace the request handler.
    """
    _listen.set_notify(_ServerListener(this, _sslctx, handler))

  be dispose() =>
    """
    Shut down the server.
    """
    _listen.dispose()

  fun local_address(): IPAddress =>
    """
    Returns the locally bound address.
    """
    _address

  be _listening(address: IPAddress) =>
    """
    Called when we are listening.
    """
    _address = address
    _notify.listening(this)

  be _not_listening() =>
    """
    Called when we fail to listen.
    """
    _notify.not_listening(this)

  be _closed() =>
    """
    Called when we stop listening.
    """
    _notify.closed(this)
