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
      ResponseTimeoutClient(auth, ssl_ctx, env.out)
    else
      env.out.print("Failed to initialize SSL context")
    end

actor ResponseTimeoutClient is HTTPClientConnectionActor
  """
  Demonstrates a response deadline using one-shot timers.

  Sends a GET request to a deliberately slow endpoint and sets a 3-second
  deadline. The timer fires before the response arrives, printing a timeout
  message and closing the connection. If the response were to arrive first,
  the timer would be cancelled.
  """
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  var _timer: (TimerToken | None) = None
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
    _out.print("Connected — sending request with 3-second deadline")
    _http.send_request(Request.get("/delay/10").build())

    // Set a 3-second response deadline. MakeTimerDuration validates
    // the millisecond value and returns a TimerDuration on success.
    match MakeTimerDuration(3_000)
    | let d: TimerDuration =>
      // set_timer returns a TimerToken on success, or a SetTimerError
      // if the connection isn't open or a timer is already active.
      match \exhaustive\ _http.set_timer(d)
      | let t: TimerToken =>
        _timer = t
      | SetTimerAlreadyActive =>
        _out.print("Timer already active")
      | SetTimerNotOpen =>
        _out.print("Connection not open")
      end
    end

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    let msg =
      match \exhaustive\ reason
      | ConnectionFailedDNS => "DNS resolution failed"
      | ConnectionFailedTCP => "TCP connection failed"
      | ConnectionFailedSSL => "SSL handshake failed"
      | ConnectionFailedTimeout => "connection timed out"
      | ConnectionFailedTimerError => "timer setup failed"
      end
    _out.print("Connection failed: " + msg)

  fun ref on_response(response: Response val) =>
    _out.print("< " + response.status.string() + " " + response.reason)

  fun ref on_response_complete() =>
    // Response arrived before the deadline — cancel the timer.
    match _timer
    | let t: TimerToken =>
      _http.cancel_timer(t)
      _timer = None
      _out.print("Response complete (timer cancelled)")
    end
    _http.close()

  fun ref on_timer(token: TimerToken) =>
    // Deadline expired before the response arrived.
    match _timer
    | let t: TimerToken if t == token =>
      _timer = None
      _out.print("Response timed out — closing connection")
      _http.close()
    end

  fun ref on_closed() =>
    _out.print("Connection closed")
