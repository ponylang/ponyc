use "net"
use uri = "uri"
use "pony_test"

class \nodoc\ iso _TestCrossOriginRedirectHTTP is UnitTest
  """
  A cross-origin redirect over HTTP reaches the second server and delivers
  the final 200 response to the user.
  """
  fun name(): String => "redirect_follower/cross_origin_redirect_http"

  fun apply(h: TestHelper) =>
    let coord = _CrossOriginCoordinator(h)
    h.dispose_when_done(coord)
    h.long_test(10_000_000_000)

actor \nodoc\ _CrossOriginCoordinator
  """
  Creates two TCP listeners. When both are listening, creates the HTTP
  client that triggers the cross-origin redirect from server 1 to server 2.
  """
  let _h: TestHelper
  var _port2: String = ""
  var _listener1: (_CrossOriginListener | None) = None
  var _listener2: (_CrossOriginListener | None) = None
  var _client: (_CrossOriginClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _listener2 = _CrossOriginListener(h, host, this, "")

  be _listener_ready(port: String, redirect_port: String) =>
    if redirect_port.size() == 0 then
      _port2 = port
      let host = ifdef linux then "127.0.0.2" else "localhost" end
      _listener1 = _CrossOriginListener(_h, host, this, _port2)
    else
      let host = ifdef linux then "127.0.0.2" else "localhost" end
      _client = _CrossOriginClient(_h, host, port, _port2)
    end

  be dispose() =>
    match _listener1
    | let l: _CrossOriginListener => l.dispose()
    end
    match _listener2
    | let l: _CrossOriginListener => l.dispose()
    end
    match _client
    | let c: _CrossOriginClient => c.dispose()
    end

actor \nodoc\ _CrossOriginListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _coord: _CrossOriginCoordinator
  let _redirect_port: String
  let _servers: Array[_CrossOriginServer] = Array[_CrossOriginServer]

  new create(
    h: TestHelper,
    host: String,
    coord: _CrossOriginCoordinator,
    redirect_port: String)
  =>
    _h = h
    _coord = coord
    _redirect_port = redirect_port
    _tcp_listener =
      TCPListener(TCPListenAuth(h.env.root), host, "0", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _CrossOriginServer =>
    let server = _CrossOriginServer(fd, _h, _redirect_port)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    let port = _tcp_listener.local_address().port().string()
    _coord._listener_ready(port, _redirect_port)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open test listener")

  fun ref _on_closed() =>
    for s in _servers.values() do s.dispose() end

actor \nodoc\ _CrossOriginServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Minimal HTTP server. When redirect_port is non-empty, responds with a 302
  pointing to that port. Otherwise responds with a 200.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _redirect_port: String
  var _responded: Bool = false

  new create(fd: U32, h: TestHelper, redirect_port: String) =>
    _h = h
    _redirect_port = redirect_port
    _tcp_connection =
      TCPConnection.server(TCPServerAuth(h.env.root), fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    _h.fail("Server start failed")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _responded then return KeepReading end
    _responded = true
    if _redirect_port.size() > 0 then
      _h.complete_action("server1 received request")
      let host = ifdef linux then "127.0.0.2" else "localhost" end
      let response =
        recover val
          String
            .> append("HTTP/1.1 302 Found\r\nLocation: http://")
            .> append(host)
            .> append(":")
            .> append(_redirect_port)
            .> append("/next\r\nContent-Length: 0\r\n\r\n")
        end
      _tcp_connection.send(response)
    else
      _h.complete_action("server2 received request")
      _tcp_connection.send("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n")
    end
    KeepReading

actor \nodoc\ _CrossOriginClient
  is (HTTPClientConnectionActor & RedirectFollowerNotify)
  var _http: RedirectFollower = RedirectFollower.none()
  let _h: TestHelper

  new create(
    h: TestHelper,
    host: String,
    port1: String,
    port2: String)
  =>
    _h = h
    h.expect_action("server1 received request")
    h.expect_action("server2 received request")
    h.expect_action("client got 200")

    let config = ClientConnectionConfig
    let conn =
      HTTPClientConnection(
        TCPConnectAuth(h.env.root), host, port1, this, config)
    let factory =
      {ref(target: uri.URI val)(auth = TCPConnectAuth(h.env.root),
        config, client = this)
        : HTTPClientConnection
      =>
        let o = Origin.from_uri(target)
        HTTPClientConnection(auth, o.host, o.port, client, config)
      }
    _http =
      RedirectFollower(
        conn, this, factory, 5, Origin(false, host, port1))

  fun ref _http_client_connection(): HTTPClientConnection =>
    _http.connection()

  fun ref on_connected() =>
    _http.send_request(Request.get("/start").build())

  fun ref on_response(response: Response val) =>
    if response.status == 200 then
      _h.complete_action("client got 200")
    else
      _h.fail("Expected 200, got " + response.status.string())
    end

  fun ref on_response_complete() =>
    _http.connection().close()

  fun ref on_closed() => None

  fun ref on_redirect_error(err: RedirectError) =>
    _h.fail("Unexpected redirect error")

  fun ref on_parse_error(err: ParseError) =>
    _h.fail("Parse error")

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("Connection failure")
