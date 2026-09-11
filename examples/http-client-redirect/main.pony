use "http_client"
use "files"
use "net"
use uri = "uri"

actor Main
  new create(env: Env) =>
    let auth = TCPConnectAuth(env.root)
    try
      let ssl_ctx =
        recover val
          SSLContext
            .> set_client_verify(true)
            .> set_authority(
              FilePath(
                FileAuth(env.root),
                "/etc/ssl/certs/ca-certificates.crt"))?
        end
      RedirectClient(auth, ssl_ctx, "httpbin.org", "443", env.out)
    else
      env.err.print("SSL context creation failed")
    end

actor RedirectClient is
  (HTTPClientConnectionActor & RedirectFollowerNotify)
  """
  HTTP client that follows redirects transparently.

  Usage: ./redirect
  Connects to httpbin.org:443, sends GET /redirect/3, and prints the final
  response after three redirect hops. Requires network access.

  The actor's response callbacks see only the final 200 — redirect hops are
  handled internally by RedirectFollower. The only redirect-specific callback
  is on_redirect_error, which fires when a hop cannot be followed.
  """
  var _http: RedirectFollower = RedirectFollower.none()
  var _collector: ResponseCollector = ResponseCollector
  let _out: OutStream

  new create(
    auth: TCPConnectAuth,
    ssl_ctx: SSLContext val,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    let config = ClientConnectionConfig

    let conn =
      HTTPClientConnection.ssl(
        auth, ssl_ctx, host, port, this, config)

    let factory =
      {ref(target: uri.URI val)
        (auth, ssl_ctx, config, client = this)
        : HTTPClientConnection
      =>
        let o = Origin.from_uri(target)
        if o.host.size() == 0 then
          return HTTPClientConnection.none()
        end
        if o.secure then
          HTTPClientConnection.ssl(
            auth, ssl_ctx, o.host, o.port, client, config)
        else
          HTTPClientConnection(
            auth, o.host, o.port, client, config)
        end
      }

    _http =
      RedirectFollower(
        conn, this, factory, 5, Origin(true, host, port))

  fun ref _http_client_connection(): HTTPClientConnection =>
    _http.connection()

  fun ref on_connected() =>
    _http.send_request(Request.get("/redirect/3").build())

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    match \exhaustive\ reason
    | ConnectionFailedDNS => _out.print("DNS resolution failed")
    | ConnectionFailedTCP => _out.print("TCP connection failed")
    | ConnectionFailedSSL => _out.print("SSL handshake failed")
    | ConnectionFailedTimeout => _out.print("Connection timed out")
    | ConnectionFailedTimerError => _out.print("Connect timer failed")
    end

  fun ref on_response(response: Response val) =>
    _collector = ResponseCollector
    _collector.set_response(response)
    _out.print("< " + response.version.string() + " " +
      response.status.string() + " " + response.reason)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _collector.add_chunk(data)

  fun ref on_response_complete() =>
    try
      let response = _collector.build()?
      _out.print(String.from_array(response.body))
    end
    _http.connection().close()

  fun ref on_redirect_error(err: RedirectError) =>
    _out.print("Redirect failed")
    _http.connection().close()

  fun ref on_closed() =>
    _out.print("Connection closed")
