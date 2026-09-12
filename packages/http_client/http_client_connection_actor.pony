use "net"

trait tag HTTPClientConnectionActor is
  (TCPConnectionActor & HTTPClientLifecycleEventReceiver)
  """
  Trait for actors that make HTTP client connections.

  Extends `TCPConnectionActor` (for net ASIO plumbing) and
  `HTTPClientLifecycleEventReceiver` (for HTTP-level callbacks). The
  actor stores an `HTTPClientConnection` as a field and implements
  `_http_client_connection()` to return it. All other required methods have
  default implementations that delegate to the protocol.

  Minimal implementation:

  ```pony
  actor MyClient is HTTPClientConnectionActor
    var _http: HTTPClientConnection = HTTPClientConnection.none()

    new create(auth: TCPConnectAuth, host: String, port: String,
      config: ClientConnectionConfig)
    =>
      _http = HTTPClientConnection(auth, host, port, this, config)

    fun ref _http_client_connection(): HTTPClientConnection => _http

    fun ref on_connected() =>
      let request = HTTPRequest(GET, "/")
      _http.send_request(request)

    fun ref on_response(response: Response val) =>
      // process response
      None
  ```

  For HTTPS, use `HTTPClientConnection.ssl(auth, ssl_ctx, host, port,
  this, config)` instead of `HTTPClientConnection(auth, host, port,
  this, config)`.

  The `none()` default ensures all fields are initialized before the
  constructor body runs, so `this` is `ref` when passed to
  `HTTPClientConnection.create()` or `HTTPClientConnection.ssl()`.
  """

  fun ref _http_client_connection(): HTTPClientConnection
    """
    Return the protocol instance owned by this actor.

    Called by the default implementation of `_connection()`. Must return
    the same instance every time.
    """

  fun ref _connection(): TCPConnection =>
    """
    Delegates to the protocol's TCP connection.
    """
    _http_client_connection()._connection()
