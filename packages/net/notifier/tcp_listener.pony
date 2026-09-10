use net = ".."

actor TCPListener is net.TCPListenerActor
  """
  A TCP listener actor driven by a
  [`TCPListenNotify`](/net/notifier-TCPListenNotify/).

  Wraps `TCPListener` and `TCPListenerActor` into a single
  actor. When a client connects, the listener asks the notifier for a
  `ServerTCPConnectionNotify` and creates a `ServerTCPConnection` internally.
  """
  var _tcp_listener: net.TCPListener = net.TCPListener.none()
  var _notify: TCPListenNotify ref
  let _server_auth: net.TCPServerAuth
  let _ssl_ctx: (net.SSLContext val | None)
  let _servers: Array[ServerTCPConnection] = Array[ServerTCPConnection]

  new create(auth: net.TCPListenAuth,
    notify: TCPListenNotify iso,
    host: String,
    service: String,
    limit: (net.MaxSpawn | None) = net.DefaultMaxSpawn(),
    ip_version: net.IPVersion = net.DualStack)
  =>
    """
    Open a plaintext TCP listener on `host`:`service`.
    """
    _notify = consume notify
    _server_auth = net.TCPServerAuth(auth)
    _ssl_ctx = None
    _tcp_listener =
      net.TCPListener(
        auth, host, service, this, ip_version, limit)

  new ssl(auth: net.TCPListenAuth,
    notify: TCPListenNotify iso,
    ctx: net.SSLContext val,
    host: String,
    service: String,
    limit: (net.MaxSpawn | None) = net.DefaultMaxSpawn(),
    ip_version: net.IPVersion = net.DualStack)
  =>
    """
    Open an SSL TCP listener on `host`:`service`. Accepted connections use
    `ctx` for the TLS handshake.
    """
    _notify = consume notify
    _server_auth = net.TCPServerAuth(auth)
    _ssl_ctx = ctx
    _tcp_listener =
      net.TCPListener(
        auth, host, service, this, ip_version, limit)

  // --- TCPListenerActor ------------------------------------------------------
  fun ref _listener(): net.TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): ServerTCPConnection ? =>
    let notify = _notify.on_connected(this)?
    let server = match _ssl_ctx
    | let ctx: net.SSLContext val =>
      ServerTCPConnection._ssl_create(_server_auth, ctx, fd, consume notify)
    else
      ServerTCPConnection._create(_server_auth, fd, consume notify)
    end
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _notify.on_listening(this)

  fun ref _on_listen_failure() =>
    _notify.on_not_listening(this)

  fun ref _on_closed() =>
    for server in _servers.values() do
      server.dispose()
    end
    _servers.clear()
    _notify.on_closed(this)

  // --- Synchronous methods ---------------------------------------------------
  fun ref local_address(): net.NetAddress =>
    """
    Return the local IP address the listener is bound to.
    """
    _tcp_listener.local_address()

  fun ref close() =>
    """
    Close the listener. No further connections are accepted. `on_closed` fires
    on the notifier.
    """
    _tcp_listener.close()
