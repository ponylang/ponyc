use "collections"
use "net"
use "net/ssl"

type Routes is Map[String, RequestHandler]

actor Server
  """
  Runs an HTTP server. When routes are changed, the changes are only reflected
  for new connections. Existing connections continue to use the old routes.
  """
  let _notify: ServerNotify
  let _sslctx: (SSLContext | None)
  var _routes: Routes
  let _listen: TCPListener
  var _address: IPAddress

  new create(notify: ServerNotify iso, host: String = "",
    service: String = "0", routes: Routes val = recover Routes end,
    sslctx: (SSLContext | None) = None)
  =>
    """
    Create a server bound to the given host and service.
    """
    _notify = consume notify
    _routes = routes.clone()
    _sslctx = sslctx
    _listen = TCPListener(_ServerListener(this, sslctx, recover Routes end),
      host, service)
    _address = recover IPAddress end

  be set_routes(routes: Routes val) =>
    """
    Replace the route map.
    """
    _routes = routes.clone()
    _listen.set_notify(_ServerListener(this, _sslctx, routes))

  be add_route(path: String, handler: RequestHandler) =>
    """
    Map a route to a handler.
    """
    let previous = _routes(path) = handler

    if previous isnt handler then
      _replace_routes()
    end

  be remove_route(path: String) =>
    """
    Unmap a route.
    """
    try
      _routes.remove(path)
      _replace_routes()
    end

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

  fun ref _replace_routes() =>
    """
    Replace the routes after modifying.
    """
    try
      let size = _routes.size()
      let routes = recover Routes(size) end

      for (k, v) in _routes.pairs() do
        routes(k) = v
      end

      _listen.set_notify(_ServerListener(this, _sslctx, consume routes))
    end
