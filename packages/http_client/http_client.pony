"""
http_client — HTTP client for Pony.

An HTTP/1.1 client library. A protocol handler class
(`HTTPClientConnection`) owned by the user's actor, with synchronous
`fun ref` callbacks. No hidden actors.

## Getting Started

Implement `HTTPClientConnectionActor` on your actor, store an
`HTTPClientConnection` as a field, and override the lifecycle callbacks
you need:

```pony
use "http_client"
use "net"

actor MyClient is HTTPClientConnectionActor
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth, host: String, port: String,
    out: OutStream)
  =>
    _out = out
    _http = HTTPClientConnection(auth, host, port, this,
      ClientConnectionConfig)

  fun ref _http_client_connection(): HTTPClientConnection => _http

  fun ref on_connected() =>
    _http.send_request(HTTPRequest(GET, "/"))

  fun ref on_response(response: Response val) =>
    _out.print(response.status.string() + " " + response.reason)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _out.write(data)

  fun ref on_response_complete() =>
    _out.print("")
    _http.close()
```

For HTTPS, use `HTTPClientConnection.ssl()` instead of
`HTTPClientConnection()`.

## One-Shot Timers

For response deadlines or application-level timeouts, use
`HTTPClientConnection.set_timer()`. Unlike idle timeout, this timer fires
unconditionally — I/O activity does not reset it. Only one timer can be active
per connection at a time. The typical pattern is a response deadline: set a
timer
after sending a request, cancel it when the response completes, close the
connection if the timer fires:

```pony
actor MyClient is HTTPClientConnectionActor
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  var _timer: (TimerToken | None) = None
  let _out: OutStream

  // ... constructor ...

  fun ref _http_client_connection(): HTTPClientConnection => _http

  fun ref on_connected() =>
    _http.send_request(Request.get("/slow-endpoint").build())
    match MakeTimerDuration(5_000)
    | let d: TimerDuration =>
      match _http.set_timer(d)
      | let t: TimerToken => _timer = t
      | let err: SetTimerError => None
      end
    end

  fun ref on_response_complete() =>
    match _timer
    | let t: TimerToken =>
      _http.cancel_timer(t)
      _timer = None
    end
    // process response...
    _http.close()

  fun ref on_timer(token: TimerToken) =>
    match _timer
    | let t: TimerToken if t == token =>
      _timer = None
      _out.print("Response timed out")
      _http.close()
    end
```

## Following Redirects

Redirect following is opt-in via `RedirectFollower`, which wraps an
`HTTPClientConnection` and intercepts redirect responses transparently. The
actor's callback code has no redirect awareness — the follower handles hops
internally and forwards only the final response.

Same-origin redirects reuse the existing TCP connection. Cross-origin redirects
use a `RedirectConnectionFactory` to create a new one.

Implement `RedirectFollowerNotify` (which extends
`HTTPClientLifecycleEventReceiver` with `on_redirect_error`), store a
`RedirectFollower` instead of an `HTTPClientConnection`, and delegate
`_http_client_connection()` to `_http.connection()`.

## Key Types

- `HTTPClientConnectionActor` — trait for your actor
- `HTTPClientConnection` — protocol handler class (stored as actor field)
- `HTTPClientLifecycleEventReceiver` — callback trait (default no-ops)
- `RedirectFollower` — wraps a connection and follows redirects transparently
- `RedirectFollowerNotify` — callback trait for actors using RedirectFollower
- `RedirectConnectionFactory` — creates connections for cross-origin redirects
- `Redirect` — a validated redirect hop (internal to RedirectFollower)
- `RedirectError` — why a redirect was not followed (`TooManyRedirects`,
  `MissingLocation`, `InvalidLocation`, `InsecureRedirect`)
- `Origin` — a URL's scheme, host, and port (the cross-origin boundary)
- `HTTPRequest` — request data (method, path, headers, body)
- `Response` — parsed response metadata (version, status, reason, headers)
- `ClientConnectionConfig` — parser limits, idle timeout, connection timeout,
  bind address
- `SendRequestResult` — result of `send_request()` (success or error)
- `ConnectionFailureReason` — reason a connection attempt failed
  (`ConnectionFailedDNS`, `ConnectionFailedTCP`, `ConnectionFailedSSL`,
  `ConnectionFailedTimeout`, `ConnectionFailedTimerError`)
- `TimerToken` — opaque token for timer cancellation and matching
- `TimerDuration` — validated timer duration (use
  `MakeTimerDuration(milliseconds)` to create)
- `SetTimerError` — timer setup failure (`SetTimerAlreadyActive`,
  `SetTimerNotOpen`)
- `HTTPResponse` — buffered response with complete body
  (from `ResponseCollector`)
- `ResponseCollector` — accumulates streaming callbacks into `HTTPResponse`
- `QueryParams` — RFC 3986 query string encoding
- `FormEncoder` — `application/x-www-form-urlencoded` body encoding
- `BasicAuth` — HTTP Basic authentication header
- `BearerAuth` — HTTP Bearer token authentication header
- `Request` — factory for typed step-builder request construction
- `RequestOptions` — builder interface for all methods (headers, query, auth)
- `RequestOptionsWithBody` — builder interface with body methods (POST, etc.)
- `MultipartFormData` — `multipart/form-data` builder for file uploads and
  mixed form submissions; use with `multipart_body()` on the request builder.
  For simple key-value form data without files, use `FormEncoder`/`form_body()`
  instead.
- `ResponseJSON` — parse `HTTPResponse` body as JSON
- `JSONDecoder` — interface for typed JSON decoders
- `JSONDecodeError` — decode failure with descriptive message
- `DecodeJSON` — parse and decode an HTTP response body in one step
"""
