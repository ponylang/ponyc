use "constrained_types"
use "files"
use "pony_test"

class \nodoc\ iso _TestConnectionTimeoutFires is UnitTest
  """
  Test that the connection timeout fires when connecting to a non-routable
  address. Connects to 192.0.2.1 (RFC 5737 TEST-NET-1) with a 2-second
  timeout.
  """
  fun name(): String => "net/ConnectionTimeoutFires"

  fun apply(h: TestHelper) =>
    let client = _TestConnectionTimeoutFiresClient(h)
    h.dispose_when_done(client)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestConnectionTimeoutFiresClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(2_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.client(
          TCPConnectAuth(_h.env.root),
          "192.0.2.1",
          "9737",
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(2_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("_on_connected for a connection that should have timed out")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedTimeout =>
      _h.complete(true)
    else
      _h.fail("Expected ConnectionFailedTimeout, got a different reason")
      _h.complete(false)
    end

class \nodoc\ iso _TestConnectionTimeoutCancelledOnConnect is UnitTest
  """
  Test that the connect timer is cancelled when a connection succeeds.
  Starts a local listener, connects a client with a long timeout, and
  verifies _on_connected fires normally.
  """
  fun name(): String => "net/ConnectionTimeoutCancelledOnConnect"

  fun apply(h: TestHelper) =>
    let listener = _TestConnectionTimeoutCancelListener(h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestConnectionTimeoutCancelListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestConnectionTimeoutCancelClient | None) = None
  let _servers: Array[_TestConnectionTimeoutCancelServer] =
    Array[_TestConnectionTimeoutCancelServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9738",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestConnectionTimeoutCancelServer =>
    let server = _TestConnectionTimeoutCancelServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestConnectionTimeoutCancelClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestConnectionTimeoutCancelClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestConnectionTimeoutCancelListener")

actor \nodoc\ _TestConnectionTimeoutCancelClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(30_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.client(
          TCPConnectAuth(_h.env.root),
          "localhost",
          "9738",
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(30_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.complete(true)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("Connection should have succeeded, got failure")
    _h.complete(false)

actor \nodoc\ _TestConnectionTimeoutCancelServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
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

class \nodoc\ iso _TestSSLConnectionTimeoutFires is UnitTest
  """
  Test that the connection timeout fires during SSL handshake. Connects an
  ssl_client to a plain TCP server — TCP connects but the SSL handshake
  stalls because the server doesn't speak TLS. Exercises the
  _hard_close_ssl_handshaking() timeout path (distinct from the plaintext
  _hard_close_connecting() path).
  """
  fun name(): String => "net/SSLConnectionTimeoutFires"

  fun apply(h: TestHelper) ? =>
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(FileAuth(h.env.root), "assets/cert.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    let listener =
      _TestSSLConnectionTimeoutFiresListener(
        consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSSLConnectionTimeoutFiresListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _sslctx: SSLContext val
  let _h: TestHelper
  var _client: (_TestSSLConnectionTimeoutFiresClient | None) = None
  let _servers: Array[_TestSSLConnectionTimeoutFiresServer] =
    Array[_TestSSLConnectionTimeoutFiresServer]

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9739",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSSLConnectionTimeoutFiresServer =>
    let server = _TestSSLConnectionTimeoutFiresServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLConnectionTimeoutFiresClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLConnectionTimeoutFiresClient(_sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLConnectionTimeoutFiresListener")

actor \nodoc\ _TestSSLConnectionTimeoutFiresClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(2_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.ssl_client(
          TCPConnectAuth(_h.env.root),
          sslctx,
          "localhost",
          "9739",
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(2_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("_on_connected for SSL connection that should have timed out")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedTimeout =>
      _h.complete(true)
    else
      _h.fail("Expected ConnectionFailedTimeout, got a different reason")
      _h.complete(false)
    end

actor \nodoc\ _TestSSLConnectionTimeoutFiresServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Plain TCP server (no SSL) — the SSL client's handshake will stall.
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

class \nodoc\ iso _TestSSLConnectionTimeoutCancelledOnConnect is UnitTest
  """
  Test that the connect timer is cancelled when an SSL handshake completes.
  Connects an ssl_client to a proper SSL server with a long timeout and
  verifies _on_connected fires. Exercises the _cancel_connect_timer() call
  in _SSLHandshaking.ssl_handshake_complete().
  """
  fun name(): String => "net/SSLConnectionTimeoutCancelledOnConnect"

  fun apply(h: TestHelper) ? =>
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(FileAuth(h.env.root), "assets/cert.pem"))?
          .> set_cert(
            FilePath(FileAuth(h.env.root), "assets/cert.pem"),
            FilePath(FileAuth(h.env.root), "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    let listener =
      _TestSSLConnectionTimeoutCancelListener(
        consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSSLConnectionTimeoutCancelListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _sslctx: SSLContext val
  let _h: TestHelper
  var _client: (_TestSSLConnectionTimeoutCancelClient | None) = None
  let _servers: Array[_TestSSLConnectionTimeoutCancelSSLServer] =
    Array[_TestSSLConnectionTimeoutCancelSSLServer]

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9740",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSSLConnectionTimeoutCancelSSLServer =>
    let server = _TestSSLConnectionTimeoutCancelSSLServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSSLConnectionTimeoutCancelClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSSLConnectionTimeoutCancelClient(_sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSSLConnectionTimeoutCancelListener")

actor \nodoc\ _TestSSLConnectionTimeoutCancelClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(30_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.ssl_client(
          TCPConnectAuth(_h.env.root),
          sslctx,
          "localhost",
          "9740",
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(30_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.complete(true)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("SSL connection should have succeeded, got failure")
    _h.complete(false)

actor \nodoc\ _TestSSLConnectionTimeoutCancelSSLServer
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

class \nodoc\ iso _TestCloseWhileConnectingWithTimeout is UnitTest
  """
  Test that close() during the connecting phase with a connect timeout armed
  cancels the timer and reports ConnectionFailedTCP, not
  ConnectionFailedTimeout.
  """
  fun name(): String => "net/CloseWhileConnectingWithTimeout"

  fun apply(h: TestHelper) =>
    let listener = _TestCloseWhileConnectingWithTimeoutListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestCloseWhileConnectingWithTimeoutClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(30_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.client(
          TCPConnectAuth(_h.env.root),
          "localhost",
          "9741",
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(30_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connecting(count: U32) =>
    _tcp_connection.close()

  fun ref _on_connected() =>
    _h.fail("_on_connected should not fire after close")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedTimeout =>
      _h.fail("Expected non-timeout failure, got ConnectionFailedTimeout")
      _h.complete(false)
    else
      _h.complete(true)
    end

actor \nodoc\ _TestCloseWhileConnectingWithTimeoutListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestCloseWhileConnectingWithTimeoutClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9741",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try
      (_client as _TestCloseWhileConnectingWithTimeoutClient).dispose()
    end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestCloseWhileConnectingWithTimeoutClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestCloseWhileConnectingWithTimeoutListener")

class \nodoc\ iso _TestHardCloseWhileConnectingWithTimeout is UnitTest
  """
  Test that hard_close() during the connecting phase with a connect timeout
  armed cancels the timer and reports ConnectionFailedTCP, not
  ConnectionFailedTimeout.
  """
  fun name(): String => "net/HardCloseWhileConnectingWithTimeout"

  fun apply(h: TestHelper) =>
    let listener = _TestHardCloseWhileConnectingWithTimeoutListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestHardCloseWhileConnectingWithTimeoutClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(30_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.client(
          TCPConnectAuth(_h.env.root),
          "localhost",
          "9742",
          "",
          this,
          this
          where connection_timeout = ct)
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(30_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connecting(count: U32) =>
    _tcp_connection.hard_close()

  fun ref _on_connected() =>
    _h.fail("_on_connected should not fire after hard_close")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedTimeout =>
      _h.fail("Expected non-timeout failure, got ConnectionFailedTimeout")
      _h.complete(false)
    else
      _h.complete(true)
    end

actor \nodoc\ _TestHardCloseWhileConnectingWithTimeoutListener
  is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestHardCloseWhileConnectingWithTimeoutClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9742",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try
      (_client as _TestHardCloseWhileConnectingWithTimeoutClient).dispose()
    end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestHardCloseWhileConnectingWithTimeoutClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail(
      "Unable to open _TestHardCloseWhileConnectingWithTimeoutListener")
