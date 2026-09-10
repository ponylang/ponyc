use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestOutgoingFails is UnitTest
  """
  Test that we get a failure callback when an outgoing connection fails
  """
  fun name(): String => "net/OutgoingFails"

  fun apply(h: TestHelper) =>
    let client = _TestOutgoingFailure(h)
    h.dispose_when_done(client)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestOutgoingFailure
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        host,
        "3245",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("_on_connected for a connection that should have failed")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.complete(true)

class \nodoc\ iso _TestPingPong is UnitTest
  """
  Test sending and receiving via a simple Ping-Pong application
  """
  fun name(): String => "net/PingPong"

  fun apply(h: TestHelper) =>
    let port = "7664"
    let pings_to_send: I32 = 100

    let listener = _TestPongerListener(port, pings_to_send, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestPinger is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_send: I32
  let _h: TestHelper

  new create(port: String,
    pings_to_send: I32,
    h: TestHelper)
  =>
    _pings_to_send = pings_to_send
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
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

actor \nodoc\ _TestPonger is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_receive: I32
  let _h: TestHelper

  new create(fd: U32,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _pings_to_receive = pings_to_receive
    _h = h

    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
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

actor \nodoc\ _TestPongerListener is TCPListenerActor
  let _port: String
  var _tcp_listener: TCPListener = TCPListener.none()
  var _pings_to_receive: I32
  let _h: TestHelper
  var _pinger: (_TestPinger | None) = None
  let _servers: Array[_TestPonger] = Array[_TestPonger]

  new create(port: String,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _port = port
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

  fun ref _on_accept(fd: U32): _TestPonger =>
    let server = _TestPonger(fd, _pings_to_receive, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_pinger as _TestPinger).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _pinger = _TestPinger(_port, _pings_to_receive, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestPongerListener")

class \nodoc\ iso _TestBasicBufferUntil is UnitTest
  fun name(): String => "net/BasicBufferUntil"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("expected data received")

    let s = _TestBasicBufferUntilListener(h)

    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestBasicBufferUntilClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9728",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _h.complete_action("client connected")
    _tcp_connection.send("hi there, how are you???")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.fail("Client shouldn't get data")
    KeepReading

actor \nodoc\ _TestBasicBufferUntilListener is TCPListenerActor
  let _h: TestHelper
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestBasicBufferUntilClient | None) = None
  let _servers: Array[_TestBasicBufferUntilServer] =
    Array[_TestBasicBufferUntilServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9728",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestBasicBufferUntilServer =>
    let server = _TestBasicBufferUntilServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestBasicBufferUntilClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestBasicBufferUntilClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestBasicBufferUntilListener")

actor \nodoc\ _TestBasicBufferUntilServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  let _h: TestHelper
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _received_count: U8 = 0

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
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
    _received_count = _received_count + 1

    if _received_count == 1 then
      _h.assert_eq[String]("hi t", String.from_array(consume data))
    elseif _received_count == 2 then
      _h.assert_eq[String]("here", String.from_array(consume data))
    elseif _received_count == 3 then
      _h.assert_eq[String](", ho", String.from_array(consume data))
    elseif _received_count == 4 then
      _h.assert_eq[String]("w ar", String.from_array(consume data))
    elseif _received_count == 5 then
      _h.assert_eq[String]("e yo", String.from_array(consume data))
    elseif _received_count == 6 then
      _h.assert_eq[String]("u???", String.from_array(consume data))
      _h.complete_action("expected data received")
      _tcp_connection.close()
    end
    KeepReading

class \nodoc\ iso _TestCanListen is UnitTest
  """
  Test that we can listen on a socket for incoming connections and that the
  `_on_listening` callback is correctly called.
  """
  fun name(): String => "net/CanListen"

  fun apply(h: TestHelper) =>
    let listener = _TestCanListenListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestCanListenListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "5786",
        this)

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    _h.fail("_on_accept shouldn't be called")
    _h.complete(false)
    _TestDoNothingServerActor(fd, _h)

  fun ref _on_listen_failure() =>
    _h.fail("listening failed")
    _h.complete(false)

  fun ref _on_listening() =>
    _h.complete(true)

  fun ref _listener(): TCPListener =>
    _tcp_listener

actor \nodoc\ _TestDoNothingServerActor
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()

  new create(fd: U32, h: TestHelper) =>
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(h.env.root),
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

class \nodoc\ iso _TestListenerLocalAddress is UnitTest
  """
  Test that `local_address()` on a listener returns the actual bound address.
  Binds to port "0" (OS-assigned) and verifies the reported port is non-zero.
  """
  fun name(): String => "net/ListenerLocalAddress"

  fun apply(h: TestHelper) =>
    let listener = _TestListenerLocalAddressListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestListenerLocalAddressListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "0",
        this)

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    _h.fail("_on_accept shouldn't be called")
    _h.complete(false)
    _TestDoNothingServerActor(fd, _h)

  fun ref _on_listen_failure() =>
    _h.fail("listening failed")
    _h.complete(false)

  fun ref _on_listening() =>
    let addr = _listener().local_address()
    _h.assert_true(addr.port() > 0)
    _h.complete(true)

  fun ref _listener(): TCPListener =>
    _tcp_listener

class \nodoc\ iso _TestHardCloseDuringReceive is UnitTest
  """
  A hard close from inside `_on_received` must break `_read`'s loop, not fall
  through to another socket read on the fd it just closed.

  The application closing when it has read what it needs is a normal pattern.
  `mute()` + `close()` in `_on_received` routes to `hard_close()`, which
  transitions the connection to `_Closed` while `_read` is still on the stack.
  `_read` must stop on that transition. If it doesn't, it reaches
  `_state.receive()` in `_Closed` — which is `_Unreachable()` — so a
  regression trips that here; in production it read a closed fd, and under
  connection churn a reused blocking fd would hang a scheduler thread.

  `_on_closed` completes the test action before the stray read would run, so a
  regression surfaces as a non-zero process exit from `_Unreachable()`, not a
  failed assertion. `HardCloseAfterFramedReceive` is the assertion-based
  companion for the buffered-delivery path.
  """
  fun name(): String => "net/HardCloseDuringReceive"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("server closed during receive")

    let s = _TestHardCloseDuringReceiveListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestHardCloseDuringReceiveListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestHardCloseDuringReceiveClient | None) = None
  let _servers: Array[_TestHardCloseDuringReceiveServer] =
    Array[_TestHardCloseDuringReceiveServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7920",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestHardCloseDuringReceiveServer =>
    let server = _TestHardCloseDuringReceiveServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestHardCloseDuringReceiveClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestHardCloseDuringReceiveClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestHardCloseDuringReceiveListener")

actor \nodoc\ _TestHardCloseDuringReceiveClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7920",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // Give the server data so its `_on_received` fires.
    match \exhaustive\ _tcp_connection.send("ping")
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("client send() failed")
      _h.complete(false)
    end

actor \nodoc\ _TestHardCloseDuringReceiveServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _closed_in_receive: Bool = false

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

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    // Hard close from inside the read callback (see the class docstring).
    _closed_in_receive = true
    _tcp_connection.mute()
    _tcp_connection.close()
    KeepReading

  fun ref _on_closed() =>
    if _closed_in_receive then
      _h.complete_action("server closed during receive")
    end

class \nodoc\ iso _TestHardCloseAfterFramedReceive is UnitTest
  """
  When two framed messages arrive in one socket read and the application
  hard_closes after the first, `_read` must not deliver the second.

  With `buffer_until` framing, `_read`'s inner loop hands over one frame at a
  time from a single buffered read. A `hard_close()` in the first frame's
  `_on_received` transitions the connection to `_Closed`; the loop must stop
  rather than deliver the buffered second frame after `_on_closed` fired.
  The delivery count is checked in a self-behavior that runs after `_read`
  returns, so a regression fails with a clear assertion, not a process exit.

  The two 4-byte frames go out in one `send()`, so on loopback they arrive in a
  single read. A split (not expected here) would leave the second frame unread
  after the close, so the test would pass without exercising the path — it can
  never fail spuriously.
  """
  fun name(): String => "net/HardCloseAfterFramedReceive"

  fun apply(h: TestHelper) =>
    h.expect_action("only first frame delivered")

    let s = _TestHardCloseAfterFramedReceiveListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestHardCloseAfterFramedReceiveListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestHardCloseAfterFramedReceiveClient | None) = None
  let _servers: Array[_TestHardCloseAfterFramedReceiveServer] =
    Array[_TestHardCloseAfterFramedReceiveServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7921",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestHardCloseAfterFramedReceiveServer =>
    let server = _TestHardCloseAfterFramedReceiveServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestHardCloseAfterFramedReceiveClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestHardCloseAfterFramedReceiveClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestHardCloseAfterFramedReceiveListener")

actor \nodoc\ _TestHardCloseAfterFramedReceiveClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7921",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // Two 4-byte frames in a single send, so both land in one server read.
    match \exhaustive\ _tcp_connection.send("AAAABBBB")
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("client send() failed")
      _h.complete(false)
    end

actor \nodoc\ _TestHardCloseAfterFramedReceiveServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _received_count: U8 = 0

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(4)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received_count = _received_count + 1
    if _received_count == 1 then
      _h.assert_eq[String]("AAAA", String.from_array(consume data))
      // Unmuted hard close inside the first frame's callback. The buffered
      // second frame must not be delivered next. The count is checked below in
      // a behavior that runs after `_read` returns.
      _tcp_connection.hard_close()
      _check_delivery_count()
    end
    KeepReading

  be _check_delivery_count() =>
    if _received_count == 1 then
      _h.complete_action("only first frame delivered")
    else
      _h.fail("second frame delivered after hard_close()")
    end

// ===========================================================================
// Fake-backend receive tests (no real sockets for I/O)
// ===========================================================================
class \nodoc\ iso _TestFakeRecvError is UnitTest
  """
  receive returns error. Verify: _on_closed fires (hard close from receive
  error). _on_received does not fire.
  """
  fun name(): String => "net/FakeRecvError"

  fun apply(h: TestHelper) =>
    h.expect_action("on_closed")

    let a = _TestFakeRecvErrorActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeRecvErrorActor
  is (TCPConnectionActor[_FBSendOkRecvError]
    & ServerLifecycleEventReceiver[_FBSendOkRecvError])
  var _tcp_connection: TCPConnection[_FBSendOkRecvError] =
    TCPConnection[_FBSendOkRecvError].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecvError].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecvError] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.fail("_on_received should not fire when receive errors")
    _h.complete(false)
    KeepReading

  fun ref _on_closed() =>
    _h.complete_action("on_closed")

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeRecvRetry is UnitTest
  """
  receive returns retry. Verify: _on_received does not fire. The connection
  waits for the next readable event.
  """
  fun name(): String => "net/FakeRecvRetry"

  fun apply(h: TestHelper) =>
    h.expect_action("started")

    let a = _TestFakeRecvRetryActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeRecvRetryActor
  is (TCPConnectionActor[_FBSendOkRecvRetry]
    & ServerLifecycleEventReceiver[_FBSendOkRecvRetry])
  var _tcp_connection: TCPConnection[_FBSendOkRecvRetry] =
    TCPConnection[_FBSendOkRecvRetry].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecvRetry].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecvRetry] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.mute()
    _h.complete_action("started")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.fail("_on_received should not fire when receive retries")
    _h.complete(false)
    KeepReading

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeRecvData is UnitTest
  """
  receive delivers "hello" on the first call, then retries. Verify:
  _on_received fires with "hello".
  """
  fun name(): String => "net/FakeRecvData"

  fun apply(h: TestHelper) =>
    h.expect_action("received_hello")

    let a = _TestFakeRecvDataActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeRecvDataActor
  is (TCPConnectionActor[_FBSendOkRecvHello]
    & ServerLifecycleEventReceiver[_FBSendOkRecvHello])
  var _tcp_connection: TCPConnection[_FBSendOkRecvHello] =
    TCPConnection[_FBSendOkRecvHello].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecvHello].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecvHello] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let s = String.from_iso_array(consume data)
    if s == "hello" then
      _tcp_connection.mute()
      _h.complete_action("received_hello")
    else
      _h.fail("expected 'hello', got '" + consume s + "'")
      _h.complete(false)
    end
    KeepReading

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeRecvFramed is UnitTest
  """
  receive delivers 10 bytes, buffer_until(4). Verify: _on_received fires
  twice with 4 bytes each.
  """
  fun name(): String => "net/FakeRecvFramed"

  fun apply(h: TestHelper) =>
    h.expect_action("received_two_frames")

    let a = _TestFakeRecvFramedActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeRecvFramedActor
  is (TCPConnectionActor[_FBSendOkRecv10]
    & ServerLifecycleEventReceiver[_FBSendOkRecv10])
  var _tcp_connection: TCPConnection[_FBSendOkRecv10] =
    TCPConnection[_FBSendOkRecv10].none()
  let _h: TestHelper
  var _received_count: USize = 0

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecv10].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
      match MakeBufferSize(4)
      | let b: BufferSize => _tcp_connection.buffer_until(b)
      else _Unreachable()
      end
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecv10] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received_count = _received_count + 1
    _h.assert_eq[USize](
      4,
      data.size(),
      "frame " + _received_count.string() + " should be 4 bytes")
    if _received_count == 2 then
      _tcp_connection.mute()
      _h.complete_action("received_two_frames")
    end
    KeepReading

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeMute is UnitTest
  """
  mute() before read loop runs prevents _on_received from firing. unmute()
  resumes delivery via _read_again.
  """
  fun name(): String => "net/FakeMute"

  fun apply(h: TestHelper) =>
    h.expect_action("received_after_unmute")

    let a = _TestFakeMuteActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeMuteActor
  is (TCPConnectionActor[_FBSendOkRecvHelloMute]
    & ServerLifecycleEventReceiver[_FBSendOkRecvHelloMute])
  var _tcp_connection: TCPConnection[_FBSendOkRecvHelloMute] =
    TCPConnection[_FBSendOkRecvHelloMute].none()
  let _h: TestHelper
  var _unmuted: Bool = false

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecvHelloMute].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecvHelloMute] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.mute()
    _check_muted()

  be _check_muted() =>
    _unmuted = true
    _tcp_connection.unmute()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if not _unmuted then
      _h.fail("_on_received fired while muted")
      _h.complete(false)
    else
      _tcp_connection.mute()
      _h.complete_action("received_after_unmute")
    end
    KeepReading

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeMuteFullBuffer is UnitTest
  """
  Muting from `_on_received` when the read buffer is full must not close the
  connection. The read loop checks `_muted` before calling `_fill()`, so a
  full buffer with nothing to read into is never reached.
  """
  fun name(): String => "net/FakeMuteFullBuffer"

  fun apply(h: TestHelper) =>
    h.expect_action("survived_mute")

    let a = _TestFakeMuteFullBufferActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeMuteFullBufferActor
  is (TCPConnectionActor[_FBSendOkRecvFillBuffer]
    & ServerLifecycleEventReceiver[_FBSendOkRecvFillBuffer])
  var _tcp_connection: TCPConnection[_FBSendOkRecvFillBuffer] =
    TCPConnection[_FBSendOkRecvFillBuffer].none()
  let _h: TestHelper
  var _muted: Bool = false
  var _received_count: USize = 0

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      match \exhaustive\ MakeReadBufferSize(16)
      | let rbs: ReadBufferSize =>
        _tcp_connection =
          TCPConnection[_FBSendOkRecvFillBuffer].server(
            TCPServerAuth(_h.env.root),
            fd,
            this,
            this,
            rbs)
      | let _: ValidationFailure =>
        _h.fail("MakeReadBufferSize(16) should succeed")
        _h.complete(false)
      end
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecvFillBuffer] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    match \exhaustive\ MakeBufferSize(4)
    | let b: BufferSize =>
      match \exhaustive\ _tcp_connection.buffer_until(b)
      | BufferUntilSet => None
      | BufferSizeAboveMinimum =>
        _h.fail("buffer_until(4) was refused")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeBufferSize(4) should succeed")
      _h.complete(false)
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received_count = _received_count + 1

    if _received_count == 1 then
      // First 4-byte frame chopped from 16 bytes of data. The read buffer
      // now holds 12 bytes in a 12-byte array — completely full. Muting here
      // is the state the bug (lori#324) turned into a hard close.
      _muted = true
      _tcp_connection.mute()
      _deferred_unmute()
    else
      // Reached here after unmute — the connection survived.
      _h.complete_action("survived_mute")
    end
    KeepReading

  be _deferred_unmute() =>
    _muted = false
    _tcp_connection.unmute()

  fun ref _on_closed() =>
    if _muted then
      _h.fail("connection closed while muted with a full read buffer")
      _h.complete(false)
    end

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeYieldReading is UnitTest
  """
  _on_received returns YieldReading. Verify: no further delivery until
  _read_again runs in a subsequent behavior turn.
  """
  fun name(): String => "net/FakeYieldReading"

  fun apply(h: TestHelper) =>
    h.expect_action("yielded_then_resumed")

    let a = _TestFakeYieldReadingActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeYieldReadingActor
  is (TCPConnectionActor[_FBSendOkRecv10Yield]
    & ServerLifecycleEventReceiver[_FBSendOkRecv10Yield])
  var _tcp_connection: TCPConnection[_FBSendOkRecv10Yield] =
    TCPConnection[_FBSendOkRecv10Yield].none()
  let _h: TestHelper
  var _received_count: USize = 0
  var _yielded: Bool = false

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecv10Yield].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
      match MakeBufferSize(5)
      | let b: BufferSize => _tcp_connection.buffer_until(b)
      else _Unreachable()
      end
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendOkRecv10Yield] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received_count = _received_count + 1
    if _received_count == 1 then
      _yielded = true
      YieldReading
    else
      if _yielded then
        _tcp_connection.mute()
        _h.complete_action("yielded_then_resumed")
      else
        _h.fail("second delivery without yielding first")
        _h.complete(false)
      end
      KeepReading
    end

  be dispose() =>
    _tcp_connection.hard_close()

// ---------------------------------------------------------------------------
// Client connection fake-backend tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestFakeConnectDNSFailure is UnitTest
  """
  connect returns 0 (no addresses resolved). _on_connection_failure fires
  with ConnectionFailedDNS.
  """
  fun name(): String => "net/FakeConnectDNSFailure"

  fun apply(h: TestHelper) =>
    h.expect_action("connection_failed_dns")

    let a = _TestFakeConnectDNSFailureActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeConnectDNSFailureActor
  is (TCPConnectionActor[_FBConnect0]
    & ClientLifecycleEventReceiver[_FBConnect0])
  var _tcp_connection: TCPConnection[_FBConnect0] =
    TCPConnection[_FBConnect0].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBConnect0].client(
        TCPConnectAuth(_h.env.root),
        "fake-host",
        "12345",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection[_FBConnect0] =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("_on_connected should not fire for DNS failure")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedDNS =>
      _h.complete_action("connection_failed_dns")
    else
      _h.fail("expected ConnectionFailedDNS")
      _h.complete(false)
    end

class \nodoc\ iso _TestFakeConnectInflight is UnitTest
  """
  connect returns 3 (three inflight attempts). _on_connecting fires with
  count 3.
  """
  fun name(): String => "net/FakeConnectInflight"

  fun apply(h: TestHelper) =>
    h.expect_action("on_connecting_3")

    let a = _TestFakeConnectInflightActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeConnectInflightActor
  is (TCPConnectionActor[_FBConnect3]
    & ClientLifecycleEventReceiver[_FBConnect3])
  var _tcp_connection: TCPConnection[_FBConnect3] =
    TCPConnection[_FBConnect3].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBConnect3].client(
        TCPConnectAuth(_h.env.root),
        "fake-host",
        "12345",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection[_FBConnect3] =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connecting(inflight_connections: U32) =>
    if inflight_connections == 3 then
      _h.complete_action("on_connecting_3")
    else
      _h.fail("expected inflight_connections == 3, got " +
        inflight_connections.string())
      _h.complete(false)
    end

// ---------------------------------------------------------------------------
// State machine fake-backend tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestFakeSendWhileConnecting is UnitTest
  """
  send() in _ClientConnecting returns SendErrorNotConnected.
  """
  fun name(): String => "net/FakeSendWhileConnecting"

  fun apply(h: TestHelper) =>
    h.expect_action("send_rejected")

    let a = _TestFakeSendWhileConnectingActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendWhileConnectingActor
  is (TCPConnectionActor[_FBConnect1]
    & ClientLifecycleEventReceiver[_FBConnect1])
  var _tcp_connection: TCPConnection[_FBConnect1] =
    TCPConnection[_FBConnect1].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBConnect1].client(
        TCPConnectAuth(_h.env.root),
        "fake-host",
        "12345",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection[_FBConnect1] =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connecting(inflight_connections: U32) =>
    match \exhaustive\ _tcp_connection.send("aaa")
    | SendAccepted =>
      _h.fail("send should not succeed while connecting")
      _h.complete(false)
    | SendErrorNotConnected =>
      _h.complete_action("send_rejected")
    | SendErrorNotWriteable =>
      _h.fail("send returned SendErrorNotWriteable")
      _h.complete(false)
    end

class \nodoc\ iso _TestFakeSendWhileUnconnectedClosing is UnitTest
  """
  send() in _UnconnectedClosing returns SendErrorNotConnected.
  """
  fun name(): String => "net/FakeSendWhileUnconnectedClosing"

  fun apply(h: TestHelper) =>
    h.expect_action("send_rejected")

    let a = _TestFakeSendWhileUnconnectedClosingActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendWhileUnconnectedClosingActor
  is (TCPConnectionActor[_FBConnect1]
    & ClientLifecycleEventReceiver[_FBConnect1])
  var _tcp_connection: TCPConnection[_FBConnect1] =
    TCPConnection[_FBConnect1].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBConnect1].client(
        TCPConnectAuth(_h.env.root),
        "fake-host",
        "12345",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection[_FBConnect1] =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connecting(inflight_connections: U32) =>
    _tcp_connection.close()
    match \exhaustive\ _tcp_connection.send("aaa")
    | SendAccepted =>
      _h.fail("send should not succeed in _UnconnectedClosing")
      _h.complete(false)
    | SendErrorNotConnected =>
      _h.complete_action("send_rejected")
    | SendErrorNotWriteable =>
      _h.fail("send returned SendErrorNotWriteable")
      _h.complete(false)
    end

class \nodoc\ iso _TestFakeHardCloseDuringUnconnectedClosing is UnitTest
  """
  hard_close() during _UnconnectedClosing fires _on_connection_failure
  without waiting for stragglers.
  """
  fun name(): String => "net/FakeHardCloseDuringUnconnectedClosing"

  fun apply(h: TestHelper) =>
    h.expect_action("connection_failed")

    let a = _TestFakeHardCloseDuringUnconnectedClosingActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeHardCloseDuringUnconnectedClosingActor
  is (TCPConnectionActor[_FBConnect2]
    & ClientLifecycleEventReceiver[_FBConnect2])
  var _tcp_connection: TCPConnection[_FBConnect2] =
    TCPConnection[_FBConnect2].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBConnect2].client(
        TCPConnectAuth(_h.env.root),
        "fake-host",
        "12345",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection[_FBConnect2] =>
    _tcp_connection

  fun ref _on_connecting(inflight_connections: U32) =>
    _tcp_connection.close()
    _tcp_connection.hard_close()

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.complete_action("connection_failed")

// ---------------------------------------------------------------------------
// Listener fake-backend tests
// ---------------------------------------------------------------------------
actor \nodoc\ _TestFakeServerStub[TCP: TCPBackend ref]
  is (TCPConnectionActor[TCP] & ServerLifecycleEventReceiver[TCP])
  """
  Minimal server connection for listener accept tests. Closes the fd via
  the fake backend on dispose.
  """
  var _tcp_connection: TCPConnection[TCP] = TCPConnection[TCP].none()

  new create(fd: U32, h: TestHelper) =>
    _tcp_connection =
      TCPConnection[TCP].server(
        TCPServerAuth(h.env.root),
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection[TCP] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.mute()

class \nodoc\ iso _TestFakeListenFailure is UnitTest
  """
  listen returns a null event. _on_listen_failure fires.
  """
  fun name(): String => "net/FakeListenFailure"

  fun apply(h: TestHelper) =>
    h.expect_action("listen_failed")

    let a = _TestFakeListenFailureActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeListenFailureActor is TCPListenerActor[_FBListenFail]
  var _tcp_listener: TCPListener[_FBListenFail] =
    TCPListener[_FBListenFail].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener[_FBListenFail](
        TCPListenAuth(_h.env.root),
        "fake-host",
        "12345",
        this)

  fun ref _listener(): TCPListener[_FBListenFail] =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestFakeServerStub[_FBListenFail] ? =>
    _h.fail("_on_accept should not fire")
    error

  fun ref _on_listen_failure() =>
    _h.complete_action("listen_failed")

class \nodoc\ iso _TestFakeAccept is UnitTest
  """
  listen succeeds and accept returns one connection. _on_accept fires
  with the fd.
  """
  fun name(): String => "net/FakeAccept"

  fun apply(h: TestHelper) =>
    h.expect_action("accepted")

    let a = _TestFakeAcceptActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeAcceptActor is TCPListenerActor[_FBListenOk]
  var _tcp_listener: TCPListener[_FBListenOk] =
    TCPListener[_FBListenOk].none()
  let _h: TestHelper
  var _accepted: (_TestFakeServerStub[_FBListenOk] | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener[_FBListenOk](
        TCPListenAuth(_h.env.root),
        "fake-host",
        "12345",
        this)

  fun ref _listener(): TCPListener[_FBListenOk] =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestFakeServerStub[_FBListenOk] =>
    let stub = _TestFakeServerStub[_FBListenOk](fd, _h)
    _accepted = stub
    _h.complete_action("accepted")
    stub

  fun ref _on_listening() =>
    _tcp_listener._accept()

  fun ref _on_closed() =>
    try (_accepted as _TestFakeServerStub[_FBListenOk]).dispose() end

class \nodoc\ iso _TestFakeConnectionLimit is UnitTest
  """
  Listener with max_spawn 2 pauses after accepting 2 connections. Closing
  one resumes accepting.
  """
  fun name(): String => "net/FakeConnectionLimit"

  fun apply(h: TestHelper) =>
    h.expect_action("accept_1")
    h.expect_action("accept_2")
    h.expect_action("accept_3")

    let a = _TestFakeConnectionLimitActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeConnectionLimitActor
  is TCPListenerActor[_FBListenOkMulti]
  var _tcp_listener: TCPListener[_FBListenOkMulti] =
    TCPListener[_FBListenOkMulti].none()
  let _h: TestHelper
  var _accept_count: U32 = 0
  var _first_accepted:
    (_TestFakeServerStub[_FBListenOkMulti] | None) = None
  var _second_accepted:
    (_TestFakeServerStub[_FBListenOkMulti] | None) = None
  var _third_accepted:
    (_TestFakeServerStub[_FBListenOkMulti] | None) = None

  new create(h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeMaxSpawn(2)
    | let limit: MaxSpawn =>
      _tcp_listener =
        TCPListener[_FBListenOkMulti](
          TCPListenAuth(_h.env.root),
          "fake-host",
          "12345",
          this where limit = limit)
    | let _: ValidationFailure =>
      _h.fail("MakeMaxSpawn(2) failed")
    end

  fun ref _listener(): TCPListener[_FBListenOkMulti] =>
    _tcp_listener

  fun ref _on_accept(fd: U32)
    : _TestFakeServerStub[_FBListenOkMulti]
  =>
    _accept_count = _accept_count + 1
    let stub = _TestFakeServerStub[_FBListenOkMulti](fd, _h)
    match _accept_count
    | 1 =>
      _first_accepted = stub
      _h.complete_action("accept_1")
    | 2 =>
      _second_accepted = stub
      _h.complete_action("accept_2")
    | 3 =>
      _third_accepted = stub
      _h.complete_action("accept_3")
    end
    stub

  fun ref _on_listening() =>
    _tcp_listener._accept()
    _simulate_close()

  be _simulate_close() =>
    _tcp_listener._connection_closed()

  fun ref _on_closed() =>
    try
      (_first_accepted as _TestFakeServerStub[_FBListenOkMulti]).dispose()
    end
    try
      (_second_accepted as _TestFakeServerStub[_FBListenOkMulti]).dispose()
    end
    try
      (_third_accepted as _TestFakeServerStub[_FBListenOkMulti]).dispose()
    end
