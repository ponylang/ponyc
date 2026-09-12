use "http_client"
use "net"

actor Main
  new create(env: Env) =>
    let auth = TCPConnectAuth(env.root)
    BasicClient(auth, "example.com", "80", env.out)

actor BasicClient is HTTPClientConnectionActor
  """
  Simple HTTP client that sends a GET request and prints the response.

  Usage: ./basic
  Connects to example.com:80, sends GET /, prints status and body.
  Requires network access. The program exits after receiving the response.

  Demonstrates `ResponseCollector` to accumulate streaming chunks into a
  single `HTTPResponse` with the complete body.
  """
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  var _collector: ResponseCollector = ResponseCollector
  let _out: OutStream

  new create(
    auth: TCPConnectAuth,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _http =
      HTTPClientConnection(
        auth, host, port, this, ClientConnectionConfig)

  fun ref _http_client_connection(): HTTPClientConnection => _http

  fun ref on_connected() =>
    _http.send_request(Request.get("/").build())

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    match \exhaustive\ reason
    | ConnectionFailedDNS => _out.print("DNS resolution failed")
    | ConnectionFailedTCP => _out.print("TCP connection failed")
    | ConnectionFailedSSL => _out.print("SSL handshake failed")
    | ConnectionFailedTimeout => _out.print("Connection timed out")
    | ConnectionFailedTimerError => _out.print("Connect timer failed")
    end

  fun ref on_response(response: Response val) =>
    """
    Handle response metadata and begin collecting body chunks.
    """
    _collector = ResponseCollector
    _collector.set_response(response)
    _out.print("< " + response.version.string() + " " +
      response.status.string() + " " + response.reason)
    for (name, value) in response.headers.values() do
      _out.print("< " + name + ": " + value)
    end
    _out.print("")

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
