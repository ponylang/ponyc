use "constrained_types"
use "files"
use "pony_test"

class \nodoc\ iso _TestYieldRead is UnitTest
  """
  Test that returning `YieldReading` exits the read loop without losing data
  and that reading resumes automatically in the next scheduler turn.

  The client sends all twenty messages in one write, so they all come out of a
  single TCP read. On the first, the server sends itself a behavior.

  A `YieldReading` the loop ignored would deliver all twenty inside the behavior
  that sent that marker, so the marker could not have run by the last one. A
  loop that honors it returns after each message, and the marker runs in a
  behavior of its own.

  The check is on the last message rather than the second, because a read
  behavior already in the actor's queue can deliver the second one before the
  marker gets a turn. `_complete_server_initialization` queues exactly such a
  read, and its own comment says it races ASIO. Each read that runs delivers at
  most one message — it yields too — so the marker cannot be starved for long.
  """
  fun name(): String => "net/YieldRead"

  fun apply(h: TestHelper) =>
    let listener = _TestYieldReadListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestYieldReadListener is TCPListenerActor
  let _h: TestHelper
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestYieldReadClient | None) = None
  let _servers: Array[_TestYieldReadServer] = Array[_TestYieldReadServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7900",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestYieldReadServer =>
    let server = _TestYieldReadServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestYieldReadClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestYieldReadClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestYieldReadListener")

actor \nodoc\ _TestYieldReadClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7900",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // One send, so all twenty messages come out of a single TCP read. Twenty
    // separate sends could arrive as separate reads, and a broken yield would
    // then get a behavior boundary it did not earn.
    _tcp_connection.send(
      "PingPingPingPingPingPingPingPingPingPing" +
      "PingPingPingPingPingPingPingPingPingPing")

actor \nodoc\ _TestYieldReadServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _received_count: USize = 0
  var _marked: Bool = false

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match \exhaustive\ MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    | let _: ValidationFailure =>
      _h.fail("MakeBufferSize(4) should succeed")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  be mark() =>
    _marked = true

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received_count = _received_count + 1

    if _received_count == 1 then
      mark()
    end

    if _received_count == 20 then
      if not _marked then
        _h.fail("all 20 messages were delivered before the yield took effect")
        _h.complete(false)
      else
        _h.complete(true)
      end
      _tcp_connection.close()
    end

    YieldReading

class \nodoc\ iso _TestSSLYieldRead is UnitTest
  """
  Test that `YieldReading` on an SSL connection yields after the message that
  returned it, not after every message decrypted from the same TCP read.

  The client sends 20 bytes as one SSL record. With `buffer_until(4)` that is
  five messages out of one read. On the first, the server sends itself a
  behavior and then returns `YieldReading`. A `YieldReading` the loop ignored
  would deliver all five inside the behavior that sent that marker, so the
  marker could not have run by the last one.

  The check is on the last message, not the second: a read behavior already in
  the actor's queue can deliver the second before the marker gets a turn. See
  `_TestYieldRead`.

  This assumes the 20 bytes arrive in one TCP read, which they do on loopback.
  If they were split the test would still pass without having tested anything.
  """
  fun name(): String => "net/SSLYieldRead"

  fun apply(h: TestHelper) ? =>
    let port = "7903"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    let listener = _TestSSLYieldReadListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSSLYieldReadListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLYieldReadClient | None) = None
  let _servers: Array[_TestSSLYieldReadServer] = Array[_TestSSLYieldReadServer]

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSSLYieldReadServer =>
    let s = _TestSSLYieldReadServer(_sslctx, fd, _h)
    _servers.push(s)
    s

  fun ref _on_listening() =>
    _client = _TestSSLYieldReadClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLYieldReadListener")
    _h.complete(false)

  be dispose() =>
    try (_client as _TestSSLYieldReadClient).dispose() end
    for server in _servers.values() do server.dispose() end
    _tcp_listener.close()

actor \nodoc\ _TestSSLYieldReadClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_client(
        TCPConnectAuth(h.env.root),
        sslctx,
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _tcp_connection.send("AAAABBBBCCCCDDDDEEEE")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")
    _h.complete(false)

actor \nodoc\ _TestSSLYieldReadServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _received: USize = 0
  var _marked: Bool = false

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    match \exhaustive\ MakeBufferSize(4)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    | let _: ValidationFailure =>
      _h.fail("MakeBufferSize(4) should succeed")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  be mark() =>
    _marked = true

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received = _received + 1

    if _received == 1 then
      mark()
      return YieldReading
    elseif _received == 5 then
      if not _marked then
        _h.fail("all 5 messages were delivered before the yield took effect")
        _h.complete(false)
      else
        _h.complete(true)
      end
    end

    KeepReading
