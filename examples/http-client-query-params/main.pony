use "http_client"
use "files"
use "net"

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
      QueryParamsClient(auth, ssl_ctx, env.out)
    else
      env.out.print("Failed to initialize SSL context")
    end

actor QueryParamsClient is HTTPClientConnectionActor
  """
  HTTP client that sends a GET request with query parameters.

  Usage: ./query-params
  Connects to httpbin.org over HTTPS and sends a GET request with
  percent-encoded query parameters appended to the path.
  httpbin.org echoes the query parameters back in a JSON response.

  Demonstrates the `Request` builder's `.query()` method for adding
  percent-encoded query parameters to GET requests.
  """
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  var _collector: ResponseCollector = ResponseCollector
  let _out: OutStream

  new create(
    auth: TCPConnectAuth,
    ssl_ctx: SSLContext val,
    out: OutStream)
  =>
    _out = out
    _http =
      HTTPClientConnection.ssl(
        auth,
        ssl_ctx,
        "httpbin.org",
        "443",
        this,
        ClientConnectionConfig)

  fun ref _http_client_connection(): HTTPClientConnection => _http

  fun ref on_connected() =>
    let req = Request.get("/get")
      .query("language", "pony")
      .query("topic", "http client")
      .header("Accept", "application/json")
      .build()
    _http.send_request(req)

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Connection failed")

  fun ref on_response(response: Response val) =>
    _collector = ResponseCollector
    _collector.set_response(response)
    _out.print("< " + response.status.string() + " " + response.reason)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _collector.add_chunk(data)

  fun ref on_response_complete() =>
    try
      let response = _collector.build()?
      _out.print(String.from_array(response.body))
    end
    _http.close()

  fun ref on_closed() =>
    _out.print("Connection closed")
