use "net"
use uri = "uri"

primitive _Suppressing

class RedirectFollower is HTTPClientLifecycleEventReceiver
  """
  Follows HTTP redirects transparently by sitting in the callback path
  between an HTTPClientConnection and the user's receiver.

  The connection delivers callbacks to the follower; the follower handles
  redirects internally and forwards only non-redirect events to the user.
  The user's callback code sees only the final non-redirect response.

  Same-origin redirects reuse the existing TCP connection. Cross-origin
  redirects use a RedirectConnectionFactory to create a new one.

  Use `none()` as the field default so that `this` is `ref` in the actor
  constructor body, then replace with `create()`:

  ```pony
  actor MyClient is (HTTPClientConnectionActor & RedirectFollowerNotify)
    var _http: RedirectFollower = RedirectFollower.none()

    new create(auth: TCPConnectAuth, ssl_ctx: SSLContext val,
      host: String, port: String)
    =>
      let config = ClientConnectionConfig
      let conn = HTTPClientConnection.ssl(
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
      _http = RedirectFollower(conn, this, factory, 5,
        Origin(true, host, port))

    fun ref _http_client_connection(): HTTPClientConnection =>
      _http.connection()
  ```
  """
  var _conn: HTTPClientConnection
  let _receiver: RedirectFollowerNotify ref
  let _factory: RedirectConnectionFactory
  var _remaining: USize
  var _origin: Origin
  var _last_request: (HTTPRequest val | None) = None
  var _pending: (Redirect | _Suppressing | None) = None

  new create(
    conn: HTTPClientConnection,
    receiver: RedirectFollowerNotify ref,
    factory: RedirectConnectionFactory,
    max_redirects: USize,
    origin: Origin)
  =>
    """
    Create a redirect follower that wraps `conn`.

    Inserts itself into the connection's callback path so redirect responses
    are handled internally. Non-redirect responses forward to `receiver`.
    Cross-origin redirects use `factory` to create a new connection.
    `max_redirects` is how many hops to follow before refusing with
    TooManyRedirects. `origin` is the scheme, host, and port of the initial
    connection — used to detect cross-origin hops.
    """
    _conn = conn
    _receiver = receiver
    _factory = factory
    _remaining = max_redirects
    _origin = origin
    conn._set_lifecycle_receiver(this)

  new _for_test(
    conn': HTTPClientConnection,
    receiver': RedirectFollowerNotify ref,
    factory': RedirectConnectionFactory,
    max_redirects': USize,
    origin': Origin,
    last_request': (HTTPRequest val | None) = None)
  =>
    """
    Test constructor that accepts pre-set state without installing the
    follower into a live connection.
    """
    _conn = conn'
    _receiver = receiver'
    _factory = factory'
    _remaining = max_redirects'
    _origin = origin'
    _last_request = last_request'

  new none() =>
    """
    Placeholder for the actor field default, replaced in the actor
    constructor body where `this` is available as `ref`.
    """
    _conn = HTTPClientConnection.none()
    _receiver = object ref is RedirectFollowerNotify end
    _factory =
      object ref is RedirectConnectionFactory
        fun ref apply(target: uri.URI val): HTTPClientConnection =>
          HTTPClientConnection.none()
      end
    _remaining = 0
    _origin = Origin(false, "", "")

  fun ref connection(): HTTPClientConnection =>
    """
    The current underlying connection, which changes after a cross-origin
    redirect.
    """
    _conn

  fun ref send_request(request: HTTPRequest val): SendRequestResult =>
    """
    Send a request and remember it for redirect decisions.
    """
    _last_request = request
    _conn.send_request(request)

  fun ref on_connected() =>
    match _pending = None
    | let r: Redirect =>
      _last_request = r.request()
      match _conn.send_request(r.request())
      | ResponsePending => _Unreachable()
      | ConnectionClosed => None // OS send failure; on_closed already fired
      end
    else
      _receiver.on_connected()
    end

  fun ref on_response(response: Response val) =>
    match _last_request
    | let sent: HTTPRequest val =>
      match \exhaustive\ _RedirectDecision(
        sent, _origin, response, _remaining)
      | let r: Redirect =>
        _pending = r
        return
      | let err: RedirectError =>
        _pending = _Suppressing
        _receiver.on_redirect_error(err)
        return
      | _NotARedirect => None
      end
    end
    _receiver.on_response(response)

  fun ref on_body_chunk(data: Array[U8] val) =>
    if _pending is None then
      _receiver.on_body_chunk(data)
    end

  fun ref on_response_complete() =>
    match _pending = None
    | _Suppressing => return
    | let r: Redirect =>
      let target_origin = Origin.from_uri(r.target())
      if _origin == target_origin then
        _remaining = r.remaining()
        _last_request = r.request()
        match _conn.send_request(r.request())
        | ResponsePending => _Unreachable()
        | ConnectionClosed => None // OS send failure; on_closed already fired
        end
      else
        _pending = r
        _origin = target_origin
        _remaining = r.remaining()
        _conn.close()
        _conn = _factory(r.target())
        _conn._set_lifecycle_receiver(this)
      end
    else
      _receiver.on_response_complete()
    end

  fun ref on_closed() =>
    match _pending
    | let _: Redirect => None
    else
      _receiver.on_closed()
    end

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    _receiver.on_connection_failure(reason)

  fun ref on_parse_error(err: ParseError) =>
    _receiver.on_parse_error(err)

  fun ref on_throttled() =>
    _receiver.on_throttled()

  fun ref on_unthrottled() =>
    _receiver.on_unthrottled()

  fun ref on_timer(token: TimerToken) =>
    _receiver.on_timer(token)

  fun ref on_timer_failure() =>
    _receiver.on_timer_failure()
