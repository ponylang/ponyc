use "constrained_types"
use "files"
use "pony_test"
use "time"

class \nodoc\ iso _TestSSLPingPong is UnitTest
  """
  Test SSL via the built-in ssl_client/ssl_server constructors.
  """
  fun name(): String => "net/SSLPingPong"

  fun apply(h: TestHelper) ? =>
    let port = "1417"
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

    let pings_to_send: I32 = 100

    let listener =
      _TestSSLPongerListener(
        port, consume sslctx, pings_to_send, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLPinger
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_send: I32
  let _h: TestHelper

  new create(port: String,
    sslctx: SSLContext val,
    pings_to_send: I32,
    h: TestHelper)
  =>
    _pings_to_send = pings_to_send
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
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    if _pings_to_send > 0 then
      _tcp_connection.send("Ping")
      _pings_to_send = _pings_to_send - 1
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _pings_to_send > 0 then
      _tcp_connection.send("Ping")
      _pings_to_send = _pings_to_send - 1
    elseif _pings_to_send == 0 then
      _h.complete(true)
    else
      _h.fail("Too many pongs received")
    end
    KeepReading

actor \nodoc\ _TestSSLPonger
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_receive: I32
  let _h: TestHelper

  new create(sslctx: SSLContext val,
    fd: U32,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _pings_to_receive = pings_to_receive
    _h = h

    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _pings_to_receive > 0 then
      _tcp_connection.send("Pong")
      _pings_to_receive = _pings_to_receive - 1
    elseif _pings_to_receive == 0 then
      _tcp_connection.send("Pong")
    else
      _h.fail("Too many pings received")
    end
    KeepReading

actor \nodoc\ _TestSSLPongerListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  var _pings_to_receive: I32
  let _h: TestHelper
  var _pinger: (_TestSSLPinger | None) = None
  let _servers: Array[_TestSSLPonger] = Array[_TestSSLPonger]

  new create(port: String,
    sslctx: SSLContext val,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _port = port
    _sslctx = sslctx
    _pings_to_receive = pings_to_receive
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSSLPonger =>
    let server = _TestSSLPonger(_sslctx, fd, _pings_to_receive, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_pinger as _TestSSLPinger).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _pinger =
      _TestSSLPinger(
        _port, _sslctx, _pings_to_receive, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLPongerListener")

class \nodoc\ iso _TestSSLSendv is UnitTest
  """
  Test send() with multiple buffers over an SSL connection. Client sends
  multiple buffers, server verifies the received data.
  """
  fun name(): String => "net/SSLSendv"

  fun apply(h: TestHelper) ? =>
    let port = "7896"
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

    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("data verified")
    h.expect_action("on_sent fired")

    let listener =
      _TestSSLSendvListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLSendvListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLSendvClient | None) = None
  let _servers: Array[_TestSSLSendvServer] = Array[_TestSSLSendvServer]

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
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

  fun ref _on_accept(fd: U32): _TestSSLSendvServer =>
    let server = _TestSSLSendvServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLSendvClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSSLSendvClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLSendvListener")

actor \nodoc\ _TestSSLSendvClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
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

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _h.complete_action("client connected")
    match \exhaustive\ _tcp_connection.send(
      recover val [as ByteSeq: "SSL "; "Hello"; " World"] end)
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send() returned an error")
      _h.complete(false)
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _expected_token = token

  fun ref _on_sent(token: SendToken) =>
    match \exhaustive\ _expected_token
    | let expected: SendToken =>
      _h.assert_true(token == expected, "token mismatch")
      _h.complete_action("on_sent fired")
    | None =>
      _h.fail("_on_sent fired but no token was expected")
    end

actor \nodoc\ _TestSSLSendvServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper)
  =>
    _h = h

    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    match MakeBufferSize(15)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.assert_eq[String]("SSL Hello World", String.from_array(consume data))
    _h.complete_action("data verified")
    _tcp_connection.close()
    KeepReading

class \nodoc\ iso _TestSSLHandshakeFailureClient is UnitTest
  """
  Test that an SSL client whose handshake fails (peer sends garbage) gets
  `_on_connection_failure(ConnectionFailedSSL)` via
  `_hard_close_ssl_handshaking`.
  """
  fun name(): String => "net/SSLHandshakeFailureClient"

  fun apply(h: TestHelper) ? =>
    let port = "9757"
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

    h.expect_action("client failure")

    let listener =
      _TestSSLHandshakeFailureClientListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLHandshakeFailureClientListener is TCPListenerActor
  """
  Plain TCP listener that accepts connections, sends garbage to break the
  SSL handshake, and closes.
  """
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLHandshakeFailureSSLClient | None) = None
  let _servers: Array[_TestSSLHandshakeFailurePlainServer] =
    Array[_TestSSLHandshakeFailurePlainServer]

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

  fun ref _on_accept(fd: U32): _TestSSLHandshakeFailurePlainServer =>
    let server = _TestSSLHandshakeFailurePlainServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLHandshakeFailureSSLClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client =
      _TestSSLHandshakeFailureSSLClient(
        _port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLHandshakeFailureClientListener")

actor \nodoc\ _TestSSLHandshakeFailurePlainServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Plain TCP server that sends garbage bytes to break the SSL handshake.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.send("XXXXXXXXXX")
    _tcp_connection.close()

actor \nodoc\ _TestSSLHandshakeFailureSSLClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  SSL client connecting to a plain TCP server. The handshake will fail.
  """
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
    _h.fail("Should not have connected")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedSSL =>
      _h.complete_action("client failure")
    else
      _h.fail("Expected ConnectionFailedSSL")
    end

class \nodoc\ iso _TestSSLHandshakeFailureServer is UnitTest
  """
  Test that an SSL server whose handshake fails (peer sends garbage) gets
  `_on_start_failure(StartFailedSSL)` via `_hard_close_ssl_handshaking`.
  """
  fun name(): String => "net/SSLHandshakeFailureServer"

  fun apply(h: TestHelper) ? =>
    let port = "9758"
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

    h.expect_action("server failure")

    let listener =
      _TestSSLHandshakeFailureServerListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLHandshakeFailureServerListener is TCPListenerActor
  """
  Listener that creates SSL servers. A plain TCP client connects and sends
  garbage to break the SSL handshake.
  """
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLHandshakeFailurePlainClient | None) = None
  let _servers: Array[_TestSSLHandshakeFailureSSLServer] =
    Array[_TestSSLHandshakeFailureSSLServer]

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

  fun ref _on_accept(fd: U32): _TestSSLHandshakeFailureSSLServer =>
    let server = _TestSSLHandshakeFailureSSLServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLHandshakeFailurePlainClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLHandshakeFailurePlainClient(_port, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLHandshakeFailureServerListener")

actor \nodoc\ _TestSSLHandshakeFailureSSLServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  SSL server that receives garbage from a plain client. The handshake fails.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_started() =>
    _h.fail("Should not have started")

  fun ref _on_start_failure(reason: StartFailureReason) =>
    match \exhaustive\ reason
    | StartFailedSSL =>
      _h.complete_action("server failure")
    end

actor \nodoc\ _TestSSLHandshakeFailurePlainClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Plain TCP client that sends garbage to an SSL server.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("XXXXXXXXXX")
    _tcp_connection.close()

class \nodoc\ iso _TestSSLHandshakeCompleteTransitionsToOpen is UnitTest
  """
  Test that after a successful SSL handshake, the connection accepts sends.
  """
  fun name(): String => "net/SSLHandshakeCompleteTransitionsToOpen"

  fun apply(h: TestHelper) ? =>
    let port = "9759"
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

    h.expect_action("send accepted")

    let listener =
      _TestSSLTransitionToOpenListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLTransitionToOpenListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLTransitionToOpenClient | None) = None
  let _servers: Array[_TestSSLTransitionToOpenServer] =
    Array[_TestSSLTransitionToOpenServer]

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

  fun ref _on_accept(fd: U32): _TestSSLTransitionToOpenServer =>
    let server = _TestSSLTransitionToOpenServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLTransitionToOpenClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLTransitionToOpenClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLTransitionToOpenListener")

actor \nodoc\ _TestSSLTransitionToOpenClient
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

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    match \exhaustive\ _tcp_connection.send("test")
    | SendAccepted =>
      _h.complete_action("send accepted")
    | let _: SendError =>
      _h.fail("send() should return SendAccepted")
    end
    _tcp_connection.close()

actor \nodoc\ _TestSSLTransitionToOpenServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

class \nodoc\ iso _TestSSLIsWriteableDuringHandshake is UnitTest
  """
  Test that is_writeable() returns false during the initial SSL handshake
  (state: _SSLHandshaking). A plain TCP client connects to an SSL server;
  the server enters _SSLHandshaking but the handshake never completes
  because the client doesn't speak TLS. The server calls check_writeable()
  on itself from its constructor; both this and _finish_initialization are
  self-sends (FIFO), so check_writeable fires after _finish_initialization
  when the server is in _SSLHandshaking.
  """
  fun name(): String => "net/SSLIsWriteableDuringHandshake"

  fun apply(h: TestHelper) ? =>
    let port = "9763"
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

    h.expect_action("is_writeable false during ssl handshake")

    let listener =
      _TestSSLIsWriteableListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLIsWriteableListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLIsWriteablePlainClient | None) = None
  let _servers: Array[_TestSSLIsWriteableServer] =
    Array[_TestSSLIsWriteableServer]

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

  fun ref _on_accept(fd: U32): _TestSSLIsWriteableServer =>
    let server = _TestSSLIsWriteableServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestSSLIsWriteablePlainClient).dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLIsWriteablePlainClient(_port, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLIsWriteableListener")

actor \nodoc\ _TestSSLIsWriteableServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  SSL server whose handshake stalls because the peer is a plain TCP client.
  The constructor calls check_writeable() after creating the ssl_server
  TCPConnection; both _finish_initialization (from the TCPConnection
  constructor) and check_writeable are self-sends, so FIFO ordering
  guarantees check_writeable fires in _SSLHandshaking.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    check_writeable()

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  be check_writeable() =>
    _h.assert_false(
      _tcp_connection.is_writeable(),
      "is_writeable should be false during SSL handshake")
    _h.complete_action("is_writeable false during ssl handshake")
    _tcp_connection.hard_close()

  fun ref _on_started() =>
    _h.fail("SSL handshake should not complete against plain TCP client")

actor \nodoc\ _TestSSLIsWriteablePlainClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Plain TCP client (no SSL) — the SSL server's handshake will stall.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

class \nodoc\ iso _TestSSLHardCloseDuringReceive is UnitTest
  """
  Test that hard_close() from inside _on_received on an SSL connection stops
  delivery. hard_close() disposes the SSL session, and no further _on_received
  may fire.

  The client sends two frames in one send() so both TLS records are written
  and flushed together. `_read()`'s loop must not go back to the session for
  the second one. Against an ssl that is unsafe after dispose (ponylang/ssl#66)
  reading a disposed session is a segfault rather than a failed assertion.
  """
  fun name(): String => "net/SSLHardCloseDuringReceive"

  fun apply(h: TestHelper) ? =>
    let port = "9773"
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

    h.expect_action("server received")
    h.expect_action("server closed")

    let listener = _TestSSLHardCloseReceiveListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLHardCloseReceiveListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLHardCloseReceiveClient | None) = None
  let _servers: Array[_TestSSLHardCloseReceiveServer] =
    Array[_TestSSLHardCloseReceiveServer]

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

  fun ref _on_accept(fd: U32): _TestSSLHardCloseReceiveServer =>
    let server = _TestSSLHardCloseReceiveServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestSSLHardCloseReceiveClient).dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLHardCloseReceiveClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLHardCloseReceiveListener")

actor \nodoc\ _TestSSLHardCloseReceiveClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_client(
        TCPConnectAuth(_h.env.root),
        sslctx,
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // Two buffers, so two TLS records, encrypted and flushed together.
    _tcp_connection.send(recover val [as ByteSeq: "AAAA"; "BBBB"] end)

actor \nodoc\ _TestSSLHardCloseReceiveServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _receives: USize = 0

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    // hard_close() below disposes the SSL session and fires _on_closed. The
    // client's second record is still in that session, so a read loop that
    // kept running would arrive here a second time, after the close.
    _receives = _receives + 1

    if _receives > 1 then
      _h.fail("_on_received fired " + _receives.string() + " times, want 1")
    else
      _h.complete_action("server received")
      _tcp_connection.hard_close()
    end
    KeepReading

  fun ref _on_closed() =>
    _h.complete_action("server closed")

class \nodoc\ iso _TestSSLHardCloseOnConnected is UnitTest
  """
  Test that hard_close() from inside _on_connected on an SSL client is safe.
  _on_connected fires from `_ssl_poll()` via
  `_SSLHandshaking.ssl_handshake_complete()`, so the flush below it runs after
  the callback returns and must not touch the disposed session.
  """
  fun name(): String => "net/SSLHardCloseOnConnected"

  fun apply(h: TestHelper) ? =>
    let port = "9774"
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

    h.expect_action("client closed")

    let listener =
      _TestSSLHardCloseOnConnectedListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLHardCloseOnConnectedListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLHardCloseOnConnectedClient | None) = None
  let _servers: Array[_TestSSLHardCloseOnConnectedServer] =
    Array[_TestSSLHardCloseOnConnectedServer]

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

  fun ref _on_accept(fd: U32): _TestSSLHardCloseOnConnectedServer =>
    let server = _TestSSLHardCloseOnConnectedServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestSSLHardCloseOnConnectedClient).dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLHardCloseOnConnectedClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLHardCloseOnConnectedListener")

actor \nodoc\ _TestSSLHardCloseOnConnectedClient
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

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _h.complete_action("client closed")

actor \nodoc\ _TestSSLHardCloseOnConnectedServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  The client drops the connection as soon as its handshake completes, so this
  server may start, fail to start, or close. None of that is under test.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

class \nodoc\ iso _TestSSLHardCloseOnStarted is UnitTest
  """
  Test that hard_close() from inside _on_started on an SSL server is safe.
  _on_started is the server arm of
  `_SSLHandshaking.ssl_handshake_complete()`, dispatched from `_ssl_poll()`,
  so the flush below it runs after the callback returns.
  """
  fun name(): String => "net/SSLHardCloseOnStarted"

  fun apply(h: TestHelper) ? =>
    let port = "9775"
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

    h.expect_action("server closed")

    let listener = _TestSSLHardCloseOnStartedListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLHardCloseOnStartedListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLHardCloseOnStartedClient | None) = None
  let _servers: Array[_TestSSLHardCloseOnStartedServer] =
    Array[_TestSSLHardCloseOnStartedServer]

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

  fun ref _on_accept(fd: U32): _TestSSLHardCloseOnStartedServer =>
    let server = _TestSSLHardCloseOnStartedServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestSSLHardCloseOnStartedClient).dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLHardCloseOnStartedClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLHardCloseOnStartedListener")

actor \nodoc\ _TestSSLHardCloseOnStartedClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  The server drops the connection as soon as its handshake completes, so this
  client may connect, fail to connect, or close. None of that is under test.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_client(
        TCPConnectAuth(_h.env.root),
        sslctx,
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestSSLHardCloseOnStartedServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _h.complete_action("server closed")

class \nodoc\ iso _TestSSLCloseDuringReceive is UnitTest
  """
  Test that a graceful close() from inside _on_received on an SSL connection
  keeps delivering. close() moves to `_Closing`, which still receives and
  does not dispose the SSL session, so the rest of the decrypted records from
  the same read are delivered.
  """
  fun name(): String => "net/SSLCloseDuringReceive"

  fun apply(h: TestHelper) ? =>
    let port = "9777"
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

    h.expect_action("both records delivered")
    h.expect_action("server closed")

    let listener = _TestSSLCloseReceiveListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLCloseReceiveListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLCloseReceiveClient | None) = None
  let _servers: Array[_TestSSLCloseReceiveServer] =
    Array[_TestSSLCloseReceiveServer]

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

  fun ref _on_accept(fd: U32): _TestSSLCloseReceiveServer =>
    let server = _TestSSLCloseReceiveServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestSSLCloseReceiveClient).dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLCloseReceiveClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLCloseReceiveListener")

actor \nodoc\ _TestSSLCloseReceiveClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_client(
        TCPConnectAuth(_h.env.root),
        sslctx,
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // Two buffers, so two TLS records, encrypted and flushed together.
    _tcp_connection.send(recover val [as ByteSeq: "AAAA"; "BBBB"] end)

actor \nodoc\ _TestSSLCloseReceiveServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _receives: USize = 0

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _receives = _receives + 1

    match _receives
    | 1 =>
      // Graceful close leaves the SSL session alive, so the client's second
      // record still reaches us.
      _tcp_connection.close()
    | 2 =>
      _h.complete_action("both records delivered")
    else
      _h.fail("_on_received fired " + _receives.string() + " times, want 2")
    end
    KeepReading

  fun ref _on_closed() =>
    _h.complete_action("server closed")

class \nodoc\ iso _TestSSLLargePayload is UnitTest
  """
  Test an SSL payload far larger than the read buffer, framed smaller than a
  TLS record.

  The client sends 100,000 bytes in one send. That crosses the wire as several
  TLS records, none of which lines up with the server's 1000-byte frames, and
  none of which fits in one 16,384-byte read.

  So the server's read loop has to do all the things a small payload never makes
  it do: take a message, run dry, fill from the socket, feed the SSL session,
  find a frame is still short, fill again, and hand off the scheduler when it
  has read a buffer's worth. Every frame must arrive, whole and in order.
  """
  fun name(): String => "net/SSLLargePayload"

  fun apply(h: TestHelper) ? =>
    let port = "9780"
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

    let listener = _TestSSLLargePayloadListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(30_000_000_000)

actor \nodoc\ _TestSSLLargePayloadListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLLargePayloadClient | None) = None
  let _servers: Array[_TestSSLLargePayloadServer] =
    Array[_TestSSLLargePayloadServer]

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

  fun ref _on_accept(fd: U32): _TestSSLLargePayloadServer =>
    let s = _TestSSLLargePayloadServer(_sslctx, fd, _h)
    _servers.push(s)
    s

  fun ref _on_listening() =>
    _client = _TestSSLLargePayloadClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLLargePayloadListener")
    _h.complete(false)

  be dispose() =>
    try (_client as _TestSSLLargePayloadClient).dispose() end
    for server in _servers.values() do server.dispose() end
    _tcp_listener.close()

actor \nodoc\ _TestSSLLargePayloadClient
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
    // 100 frames of 1000 bytes. Frame n is filled with the byte n % 256, so a
    // frame delivered out of order or stitched together wrong is visible.
    let payload =
      recover val
        let a = Array[U8](100000)
        var frame: USize = 0
        while frame < 100 do
          var i: USize = 0
          while i < 1000 do
            a.push(frame.u8())
            i = i + 1
          end
          frame = frame + 1
        end
        a
      end
    _tcp_connection.send(payload)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")
    _h.complete(false)

actor \nodoc\ _TestSSLLargePayloadServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _frames: USize = 0

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    match \exhaustive\ MakeBufferSize(1000)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    | let _: ValidationFailure =>
      _h.fail("MakeBufferSize(1000) should succeed")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if data.size() != 1000 then
      _h.fail("frame " + _frames.string() + " was " + data.size().string() +
        " bytes, wanted 1000")
      _h.complete(false)
      return KeepReading
    end

    let want = _frames.u8()
    for b in (consume data).values() do
      if b != want then
        _h.fail("frame " + _frames.string() + " holds byte " + b.string() +
          ", wanted " + want.string())
        _h.complete(false)
        return KeepReading
      end
    end

    _frames = _frames + 1

    if _frames == 100 then
      _h.complete(true)
    end
    KeepReading

class \nodoc\ iso _TestSSLSendDuringHandshake is UnitTest
  """
  Test that send() returns SendErrorNotConnected during the initial SSL
  handshake (state: _SSLHandshaking). A plain TCP client connects to an SSL
  server; the server enters _SSLHandshaking but the handshake never completes
  because the client doesn't speak TLS. The server calls try_send() on itself
  from its constructor; both _finish_initialization and try_send are
  self-sends, so FIFO ordering guarantees try_send fires in _SSLHandshaking.
  """
  fun name(): String => "net/SSLSendDuringHandshake"

  fun apply(h: TestHelper) ? =>
    let port = "9766"
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

    h.expect_action("send blocked during handshake")

    let listener =
      _TestSSLSendDuringHandshakeListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLSendDuringHandshakeListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLSendDuringHandshakePlainClient | None) = None
  let _servers: Array[_TestSSLSendDuringHandshakeServer] =
    Array[_TestSSLSendDuringHandshakeServer]

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

  fun ref _on_accept(fd: U32): _TestSSLSendDuringHandshakeServer =>
    let server = _TestSSLSendDuringHandshakeServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try
      (_client as _TestSSLSendDuringHandshakePlainClient).dispose()
    end

  fun ref _on_listening() =>
    _client = _TestSSLSendDuringHandshakePlainClient(_port, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLSendDuringHandshakeListener")

actor \nodoc\ _TestSSLSendDuringHandshakeServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  SSL server whose handshake stalls because the peer is a plain TCP client.
  The constructor calls try_send() after creating the ssl_server
  TCPConnection; both _finish_initialization and try_send are self-sends,
  so FIFO ordering guarantees try_send fires in _SSLHandshaking.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    try_send()

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  be try_send() =>
    match \exhaustive\ _tcp_connection.send("test")
    | SendErrorNotConnected =>
      _h.complete_action("send blocked during handshake")
    | SendAccepted =>
      _h.fail("send() should not be accepted during SSL handshake")
    | let _: SendError =>
      _h.fail("Expected SendErrorNotConnected, got other SendError")
    end
    _tcp_connection.hard_close()

  fun ref _on_started() =>
    _h.fail("SSL handshake should not complete against plain TCP client")

actor \nodoc\ _TestSSLSendDuringHandshakePlainClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

class \nodoc\ iso _TestSSLCloseDuringHandshake is UnitTest
  """
  Test that close() during the initial SSL handshake (state: _SSLHandshaking)
  routes through hard_close() and fires _on_connection_failure with
  ConnectionFailedSSL. An SSL client connects to a plain TCP server; TCP
  connects but the handshake stalls. A Pony timer fires after 1 second and
  calls close() on the connection. The connection timeout is a safety net.
  """
  fun name(): String => "net/SSLCloseDuringHandshake"

  fun apply(h: TestHelper) ? =>
    let port = "9767"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("close during handshake")

    let listener =
      _TestSSLCloseDuringHandshakeListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSSLCloseDuringHandshakeListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLCloseDuringHandshakeClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

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

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLCloseDuringHandshakeClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client =
      _TestSSLCloseDuringHandshakeClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLCloseDuringHandshakeListener")

actor \nodoc\ _TestSSLCloseDuringHandshakeClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _timers: Timers

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    _timers = Timers

    match \exhaustive\ MakeConnectionTimeout(10_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.ssl_client(
          TCPConnectAuth(_h.env.root),
          sslctx,
          "localhost",
          port,
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(10_000) should succeed")
    end

    let client: _TestSSLCloseDuringHandshakeClient tag = this
    let timer =
      Timer(
        object iso is TimerNotify
          let _c: _TestSSLCloseDuringHandshakeClient tag = client
          fun ref apply(timer: Timer, count: U64): Bool =>
            _c.try_close()
            false
        end,
        1_000_000_000,
        0)
    _timers(consume timer)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("SSL handshake should not complete against plain TCP server")
    _h.complete(false)

  be try_close() =>
    _tcp_connection.close()

  be dispose() =>
    _timers.dispose()
    _tcp_connection.hard_close()

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _timers.dispose()
    match reason
    | ConnectionFailedSSL =>
      _h.complete_action("close during handshake")
    | ConnectionFailedTimeout =>
      _h.fail("Connection timed out before close() fired")
      _h.complete(false)
    else
      _h.fail("Expected ConnectionFailedSSL")
      _h.complete(false)
    end

class \nodoc\ iso _TestSSLHardCloseDuringHandshake is UnitTest
  """
  Test that hard_close() during the initial SSL handshake (state:
  _SSLHandshaking) fires _on_connection_failure with ConnectionFailedSSL.
  An SSL client connects to a plain TCP server; TCP connects but the
  handshake stalls. A Pony timer fires after 1 second and calls hard_close().
  The connection timeout is a safety net.
  """
  fun name(): String => "net/SSLHardCloseDuringHandshake"

  fun apply(h: TestHelper) ? =>
    let port = "9768"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("hard close during handshake")

    let listener =
      _TestSSLHardCloseDuringHandshakeListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSSLHardCloseDuringHandshakeListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLHardCloseDuringHandshakeClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

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

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLHardCloseDuringHandshakeClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client =
      _TestSSLHardCloseDuringHandshakeClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLHardCloseDuringHandshakeListener")

actor \nodoc\ _TestSSLHardCloseDuringHandshakeClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _timers: Timers

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    _timers = Timers

    match \exhaustive\ MakeConnectionTimeout(10_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.ssl_client(
          TCPConnectAuth(_h.env.root),
          sslctx,
          "localhost",
          port,
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(10_000) should succeed")
    end

    let client: _TestSSLHardCloseDuringHandshakeClient tag = this
    let timer =
      Timer(
        object iso is TimerNotify
          let _c: _TestSSLHardCloseDuringHandshakeClient tag = client
          fun ref apply(timer: Timer, count: U64): Bool =>
            _c.try_hard_close()
            false
        end,
        1_000_000_000,
        0)
    _timers(consume timer)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("SSL handshake should not complete against plain TCP server")
    _h.complete(false)

  be try_hard_close() =>
    _tcp_connection.hard_close()

  be dispose() =>
    _timers.dispose()
    _tcp_connection.hard_close()

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _timers.dispose()
    match reason
    | ConnectionFailedSSL =>
      _h.complete_action("hard close during handshake")
    | ConnectionFailedTimeout =>
      _h.fail("Connection timed out before hard_close() fired")
      _h.complete(false)
    else
      _h.fail("Expected ConnectionFailedSSL")
      _h.complete(false)
    end

class \nodoc\ iso _TestSSLServerHardCloseDuringHandshake is UnitTest
  """
  Test that hard_close() on an SSL server during the initial SSL handshake
  (state: _SSLHandshaking) fires _on_start_failure with StartFailedSSL.
  A plain TCP client connects to an SSL server; the server enters
  _SSLHandshaking but the handshake never completes. The server calls
  try_hard_close() on itself from its constructor; both _finish_initialization
  and try_hard_close are self-sends, so FIFO ordering guarantees
  try_hard_close fires in _SSLHandshaking.
  """
  fun name(): String => "net/SSLServerHardCloseDuringHandshake"

  fun apply(h: TestHelper) ? =>
    let port = "9769"
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

    h.expect_action("server hard close during handshake")

    let listener =
      _TestSSLServerHardCloseHandshakeListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSSLServerHardCloseHandshakeListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSSLServerHardCloseHandshakePlainClient | None) = None
  let _servers: Array[_TestSSLServerHardCloseHandshakeServer] =
    Array[_TestSSLServerHardCloseHandshakeServer]

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

  fun ref _on_accept(fd: U32): _TestSSLServerHardCloseHandshakeServer =>
    let server =
      _TestSSLServerHardCloseHandshakeServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try
      (_client as _TestSSLServerHardCloseHandshakePlainClient).dispose()
    end

  fun ref _on_listening() =>
    _client =
      _TestSSLServerHardCloseHandshakePlainClient(_port, _h)

  fun ref _on_listen_failure() =>
    _h.fail(
      "Unable to open _TestSSLServerHardCloseHandshakeListener")

actor \nodoc\ _TestSSLServerHardCloseHandshakeServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  SSL server whose handshake stalls because the peer is a plain TCP client.
  The constructor calls try_hard_close() after creating the ssl_server
  TCPConnection; both _finish_initialization and try_hard_close are
  self-sends, so FIFO ordering guarantees try_hard_close fires in
  _SSLHandshaking.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    try_hard_close()

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    match \exhaustive\ reason
    | StartFailedSSL =>
      _h.complete_action("server hard close during handshake")
    end

  be try_hard_close() =>
    _tcp_connection.hard_close()

  fun ref _on_started() =>
    _h.fail("SSL handshake should not complete against plain TCP client")

actor \nodoc\ _TestSSLServerHardCloseHandshakePlainClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(port: String, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None
