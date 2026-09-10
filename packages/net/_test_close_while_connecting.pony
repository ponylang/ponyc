use "pony_test"

class \nodoc\ iso _TestHardCloseWhileConnecting is UnitTest
  """
  Test that hard_close() during the connecting phase fires
  _on_connection_failure and prevents the connection from going live.
  """
  fun name(): String => "net/HardCloseWhileConnecting"

  fun apply(h: TestHelper) =>
    h.expect_action("connection failure")

    let listener = _TestHardCloseWhileConnectingListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestHardCloseWhileConnectingClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9735",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connecting(count: U32) =>
    _tcp_connection.hard_close()

  fun ref _on_connected() =>
    _h.fail("_on_connected should not fire after hard_close")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.complete_action("connection failure")

actor \nodoc\ _TestHardCloseWhileConnectingListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestHardCloseWhileConnectingClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9735",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestHardCloseWhileConnectingClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestHardCloseWhileConnectingClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestHardCloseWhileConnectingListener")

class \nodoc\ iso _TestCloseWhileConnecting is UnitTest
  """
  Test that close() during the connecting phase fires
  _on_connection_failure and prevents the connection from going live.
  """
  fun name(): String => "net/CloseWhileConnecting"

  fun apply(h: TestHelper) =>
    h.expect_action("connection failure")

    let listener = _TestCloseWhileConnectingListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestCloseWhileConnectingClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9736",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connecting(count: U32) =>
    _tcp_connection.close()

  fun ref _on_connected() =>
    _h.fail("_on_connected should not fire after close")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.complete_action("connection failure")

actor \nodoc\ _TestCloseWhileConnectingListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestCloseWhileConnectingClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9736",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestCloseWhileConnectingClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestCloseWhileConnectingClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestCloseWhileConnectingListener")
