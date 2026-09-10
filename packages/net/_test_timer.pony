use "constrained_types"
use "files"
use "pony_test"
use "time"

class \nodoc\ iso _TestTimerFires is UnitTest
  """
  Test that a one-shot timer fires with the correct token. Server sets a
  1-second timer in _on_started and verifies _on_timer delivers the matching
  token.

  Connect, server start, and timer firing are tracked as separate actions so
  that on a timeout PonyTest prints which stage did not complete.
  """
  fun name(): String => "net/TimerFires"

  fun apply(h: TestHelper) =>
    h.expect_action("client connected")
    h.expect_action("server started")
    h.expect_action("timer fired")

    let listener = _TestTimerFiresListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerFiresListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerFiresClient | None) = None
  let _servers: Array[_TestTimerFiresServer] = Array[_TestTimerFiresServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9746",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerFiresServer =>
    let server = _TestTimerFiresServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerFiresClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerFiresClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerFiresListener")

actor \nodoc\ _TestTimerFiresClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9746",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _h.complete_action("client connected")

actor \nodoc\ _TestTimerFiresServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected_token: (TimerToken | None) = None

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
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let t: TimerToken =>
        _expected_token = t
        _h.complete_action("server started")
      | let _: SetTimerError =>
        _h.fail("set_timer returned error")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_timer(token: TimerToken) =>
    match _expected_token
    | let t: TimerToken =>
      _h.assert_true(t == token, "token should match")
      _h.complete_action("timer fired")
    else
      _h.fail("_on_timer fired without expected token")
      _h.complete(false)
    end

class \nodoc\ iso _TestTimerCancel is UnitTest
  """
  Test that cancelling a timer prevents it from firing. Server sets a
  1-second timer and immediately cancels it. A watchdog completes the test
  after 2 seconds — if _on_timer fires, the test fails. Also verifies
  double-cancel is a no-op.
  """
  fun name(): String => "net/TimerCancel"

  fun apply(h: TestHelper) =>
    let listener = _TestTimerCancelListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerCancelListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerCancelClient | None) = None
  let _servers: Array[_TestTimerCancelServer] = Array[_TestTimerCancelServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9747",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerCancelServer =>
    let server = _TestTimerCancelServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerCancelClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerCancelClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerCancelListener")

actor \nodoc\ _TestTimerCancelClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9747",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestTimerCancelServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _timers: Timers = Timers

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
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let t: TimerToken =>
        _tcp_connection.cancel_timer(t)
        // Double-cancel should be a no-op
        _tcp_connection.cancel_timer(t)
      | let _: SetTimerError =>
        _h.fail("set_timer returned error")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end
    // Watchdog: complete the test after 2 seconds
    let server: _TestTimerCancelServer tag = this
    let timer =
      Timer(
        _TestTimerCancelWatchdog(server),
        2_000_000_000,
        0)
    _timers(consume timer)

  fun ref _on_timer(token: TimerToken) =>
    _h.fail("_on_timer fired after cancel")
    _h.complete(false)

  be _watchdog_complete() =>
    _h.complete(true)

  be dispose() =>
    _timers.dispose()
    _tcp_connection.close()

class \nodoc\ _TestTimerCancelWatchdog is TimerNotify
  let _server: _TestTimerCancelServer tag

  new iso create(server: _TestTimerCancelServer tag) =>
    _server = server

  fun ref apply(timer: Timer, count: U64): Bool =>
    _server._watchdog_complete()
    false

class \nodoc\ iso _TestTimerNotResetByIO is UnitTest
  """
  Test that the user timer is NOT reset by I/O activity (unlike idle timeout).
  Server sets a 2-second timer. Client sends data at 500ms intervals.
  The timer should fire at ~2 seconds despite the I/O activity.
  """
  fun name(): String => "net/TimerNotResetByIO"

  fun apply(h: TestHelper) =>
    h.expect_action("data received")
    h.expect_action("timer fired")

    let listener = _TestTimerNotResetByIOListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerNotResetByIOListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerNotResetByIOClient | None) = None
  let _servers: Array[_TestTimerNotResetByIOServer] =
    Array[_TestTimerNotResetByIOServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9748",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerNotResetByIOServer =>
    let server = _TestTimerNotResetByIOServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerNotResetByIOClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerNotResetByIOClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerNotResetByIOListener")

actor \nodoc\ _TestTimerNotResetByIOClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _timers: Timers = Timers
  var _sends_remaining: U32 = 6

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9748",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("ping")
    _sends_remaining = _sends_remaining - 1
    _schedule_next_send()

  fun ref _schedule_next_send() =>
    if _sends_remaining > 0 then
      let client: _TestTimerNotResetByIOClient tag = this
      let timer =
        Timer(
          _TestTimerNotResetByIOTimerNotify(client),
          500_000_000,
          0)
      _timers(consume timer)
    end

  be _send_ping() =>
    _tcp_connection.send("ping")
    _sends_remaining = _sends_remaining - 1
    _schedule_next_send()

  be dispose() =>
    _timers.dispose()
    _tcp_connection.close()

class \nodoc\ _TestTimerNotResetByIOTimerNotify is TimerNotify
  let _client: _TestTimerNotResetByIOClient tag

  new iso create(client: _TestTimerNotResetByIOClient tag) =>
    _client = client

  fun ref apply(timer: Timer, count: U64): Bool =>
    _client._send_ping()
    false

actor \nodoc\ _TestTimerNotResetByIOServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _received: Bool = false

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
    match \exhaustive\ MakeTimerDuration(2_000)
    | let d: TimerDuration =>
      match _tcp_connection.set_timer(d)
      | let _: SetTimerError =>
        _h.fail("set_timer returned error")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(2_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if not _received then
      _received = true
      _h.complete_action("data received")
    end
    KeepReading

  fun ref _on_timer(token: TimerToken) =>
    _h.assert_true(_received, "should have received data before timer fired")
    _h.complete_action("timer fired")

class \nodoc\ iso _TestSetTimerNotOpen is UnitTest
  """
  Test that set_timer returns SetTimerNotOpen on a connection that hasn't
  connected.
  """
  fun name(): String => "net/SetTimerNotOpen"

  fun apply(h: TestHelper) =>
    let conn = TCPConnection.none()
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match \exhaustive\ conn.set_timer(d)
      | let _: SetTimerNotOpen => h.assert_true(true)
      | let _: TimerToken =>
        h.fail("set_timer should return SetTimerNotOpen")
      | let _: SetTimerAlreadyActive =>
        h.fail(
          "set_timer should return SetTimerNotOpen, not SetTimerAlreadyActive")
      end
    | let _: ValidationFailure =>
      h.fail("MakeTimerDuration(1_000) should succeed")
    end

class \nodoc\ iso _TestSetTimerAlreadyActive is UnitTest
  """
  Test that set_timer returns SetTimerAlreadyActive when a timer is already
  set. After cancelling, set_timer should succeed. The second timer should
  fire.
  """
  fun name(): String => "net/SetTimerAlreadyActive"

  fun apply(h: TestHelper) =>
    let listener = _TestSetTimerAlreadyActiveListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSetTimerAlreadyActiveListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSetTimerAlreadyActiveClient | None) = None
  let _servers: Array[_TestSetTimerAlreadyActiveServer] =
    Array[_TestSetTimerAlreadyActiveServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9749",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSetTimerAlreadyActiveServer =>
    let server = _TestSetTimerAlreadyActiveServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSetTimerAlreadyActiveClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSetTimerAlreadyActiveClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSetTimerAlreadyActiveListener")

actor \nodoc\ _TestSetTimerAlreadyActiveClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9749",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestSetTimerAlreadyActiveServer
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

  fun ref _on_started() =>
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      // First set_timer should succeed
      match _tcp_connection.set_timer(d)
      | let t: TimerToken =>
        // Second set_timer should return SetTimerAlreadyActive
        match _tcp_connection.set_timer(d)
        | let _: SetTimerAlreadyActive =>
          // Cancel, then set again — should succeed
          _tcp_connection.cancel_timer(t)
        else
          _h.fail("Expected SetTimerAlreadyActive")
          _h.complete(false)
          return
        end
      else
        _h.fail("First set_timer should succeed")
        _h.complete(false)
        return
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
      return
    end
    // Now set a short timer that should fire
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match _tcp_connection.set_timer(d)
      | let _: SetTimerError =>
        _h.fail("set_timer after cancel should succeed")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_timer(token: TimerToken) =>
    _h.complete(true)

class \nodoc\ iso _TestTimerRearmFromCallback is UnitTest
  """
  Test that set_timer can be called from within _on_timer to re-arm.
  Server sets a 1-second timer. In _on_timer, increments a counter and
  re-arms. After the second firing, asserts count == 2.
  """
  fun name(): String => "net/TimerRearmFromCallback"

  fun apply(h: TestHelper) =>
    let listener = _TestTimerRearmListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerRearmListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerRearmClient | None) = None
  let _servers: Array[_TestTimerRearmServer] = Array[_TestTimerRearmServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9750",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerRearmServer =>
    let server = _TestTimerRearmServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerRearmClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerRearmClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerRearmListener")

actor \nodoc\ _TestTimerRearmClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9750",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestTimerRearmServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _fire_count: U32 = 0

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
    _set_one_second_timer()

  fun ref _set_one_second_timer() =>
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match _tcp_connection.set_timer(d)
      | let _: SetTimerError =>
        _h.fail("set_timer returned error")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_timer(token: TimerToken) =>
    _fire_count = _fire_count + 1
    if _fire_count == 2 then
      _h.assert_eq[U32](2, _fire_count)
      _h.complete(true)
    else
      _set_one_second_timer()
    end

class \nodoc\ iso _TestTimerCancelWrongToken is UnitTest
  """
  Test that cancelling with a stale token is a no-op. Server sets timer A,
  cancels it, sets timer B, then cancels with stale token A. Timer B should
  still fire.
  """
  fun name(): String => "net/TimerCancelWrongToken"

  fun apply(h: TestHelper) =>
    let listener = _TestTimerCancelWrongTokenListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerCancelWrongTokenListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerCancelWrongTokenClient | None) = None
  let _servers: Array[_TestTimerCancelWrongTokenServer] =
    Array[_TestTimerCancelWrongTokenServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9751",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerCancelWrongTokenServer =>
    let server = _TestTimerCancelWrongTokenServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerCancelWrongTokenClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerCancelWrongTokenClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerCancelWrongTokenListener")

actor \nodoc\ _TestTimerCancelWrongTokenClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9751",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestTimerCancelWrongTokenServer
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

  fun ref _on_started() =>
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let token_a: TimerToken =>
        _tcp_connection.cancel_timer(token_a)
        // Set timer B (1 second)
        match \exhaustive\ MakeTimerDuration(1_000)
        | let d2: TimerDuration =>
          match \exhaustive\ _tcp_connection.set_timer(d2)
          | let _: TimerToken =>
            // Cancel with stale token A — should be a no-op
            _tcp_connection.cancel_timer(token_a)
          | let _: SetTimerError =>
            _h.fail("set_timer B should succeed")
            _h.complete(false)
          end
        | let _: ValidationFailure =>
          _h.fail("MakeTimerDuration(1_000) should succeed")
          _h.complete(false)
        end
      | let _: SetTimerError =>
        _h.fail("set_timer A should succeed")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_timer(token: TimerToken) =>
    // Timer B should fire
    _h.complete(true)

class \nodoc\ iso _TestTimerHardCloseCleanup is UnitTest
  """
  Test that hard_close cancels active timers. Server sets a 1-second timer
  and immediately hard-closes. A watchdog completes the test after 2
  seconds — if _on_timer fires, the test fails.
  """
  fun name(): String => "net/TimerHardCloseCleanup"

  fun apply(h: TestHelper) =>
    let listener = _TestTimerHardCloseListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerHardCloseListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerHardCloseClient | None) = None
  let _servers: Array[_TestTimerHardCloseServer] =
    Array[_TestTimerHardCloseServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9752",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerHardCloseServer =>
    let server = _TestTimerHardCloseServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerHardCloseClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerHardCloseClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerHardCloseListener")

actor \nodoc\ _TestTimerHardCloseClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9752",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestTimerHardCloseServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _timers: Timers = Timers

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
    match MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      _tcp_connection.set_timer(d)
    end
    _tcp_connection.hard_close()
    // Watchdog: complete the test after 2 seconds. If _on_timer fires
    // before then, the test fails.
    let server: _TestTimerHardCloseServer tag = this
    let timer =
      Timer(
        _TestTimerHardCloseWatchdog(server),
        2_000_000_000,
        0)
    _timers(consume timer)

  fun ref _on_timer(token: TimerToken) =>
    _h.fail("_on_timer fired after hard_close")
    _h.complete(false)

  be _watchdog_complete() =>
    _h.complete(true)

  be dispose() =>
    _timers.dispose()
    _tcp_connection.close()

class \nodoc\ _TestTimerHardCloseWatchdog is TimerNotify
  let _server: _TestTimerHardCloseServer tag

  new iso create(server: _TestTimerHardCloseServer tag) =>
    _server = server

  fun ref apply(timer: Timer, count: U64): Bool =>
    _server._watchdog_complete()
    false

class \nodoc\ iso _TestTimerSetDuringClosing is UnitTest
  """
  Test that set_timer returns SetTimerNotOpen during graceful shutdown
  (_Closing). Server calls close() then tries to set a timer.
  """
  fun name(): String => "net/TimerSetDuringClosing"

  fun apply(h: TestHelper) =>
    let listener = _TestTimerSetDuringClosingListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestTimerSetDuringClosingListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestTimerSetDuringClosingClient | None) = None
  let _servers: Array[_TestTimerSetDuringClosingServer] =
    Array[_TestTimerSetDuringClosingServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9753",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestTimerSetDuringClosingServer =>
    let server = _TestTimerSetDuringClosingServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestTimerSetDuringClosingClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestTimerSetDuringClosingClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestTimerSetDuringClosingListener")

actor \nodoc\ _TestTimerSetDuringClosingClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9753",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestTimerSetDuringClosingServer
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

  fun ref _on_started() =>
    _tcp_connection.close()
    // Now in _Closing
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let _: SetTimerNotOpen =>
        _h.complete(true)
      | let _: TimerToken =>
        _h.fail("set_timer should return SetTimerNotOpen during _Closing")
        _h.complete(false)
      | let _: SetTimerAlreadyActive =>
        _h.fail(
          "set_timer should return SetTimerNotOpen, not SetTimerAlreadyActive")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end

class \nodoc\ iso _TestSetTimerNotOpenDuringSSLHandshake is UnitTest
  """
  Test that set_timer returns SetTimerNotOpen during initial SSL handshake.
  An SSL client connects to a plain TCP server (handshake stalls). The
  client calls set_timer before _on_connected. Uses a connection timeout to
  ensure the test terminates.
  """
  fun name(): String => "net/SetTimerNotOpenDuringSSLHandshake"

  fun apply(h: TestHelper) ? =>
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(FileAuth(h.env.root), "assets/cert.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    let listener = _TestSetTimerNotOpenSSLListener(consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSetTimerNotOpenSSLListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _sslctx: SSLContext val
  let _h: TestHelper
  var _client: (_TestSetTimerNotOpenSSLClient | None) = None
  let _servers: Array[_TestSetTimerNotOpenSSLServer] =
    Array[_TestSetTimerNotOpenSSLServer]

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9754",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSetTimerNotOpenSSLServer =>
    let server = _TestSetTimerNotOpenSSLServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSetTimerNotOpenSSLClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSetTimerNotOpenSSLClient(_sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSetTimerNotOpenSSLListener")

actor \nodoc\ _TestSetTimerNotOpenSSLClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeConnectionTimeout(5_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.ssl_client(
          TCPConnectAuth(_h.env.root),
          sslctx,
          "localhost",
          "9754",
          "",
          this,
          this
          where connection_timeout = ct)
      // Try to set a timer before _finish_initialization runs.
      // The state is still _ConnectionNone.
      match \exhaustive\ MakeTimerDuration(1_000)
      | let d: TimerDuration =>
        match \exhaustive\ _tcp_connection.set_timer(d)
        | let _: SetTimerNotOpen =>
          None // Expected — will verify via connection timeout
        | let _: TimerToken =>
          _h.fail(
            "set_timer should return SetTimerNotOpen during SSL handshake")
          _h.complete(false)
        | let _: SetTimerAlreadyActive =>
          _h.fail(
            "set_timer should return SetTimerNotOpen, not SetTimerAlreadyActive"
          )
          _h.complete(false)
        end
      | let _: ValidationFailure =>
        _h.fail("MakeTimerDuration(1_000) should succeed")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeConnectionTimeout(5_000) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _h.fail("SSL handshake should not complete against plain TCP server")
    _h.complete(false)

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match reason
    | ConnectionFailedTimeout =>
      _h.complete(true)
    else
      _h.fail("Expected ConnectionFailedTimeout, got a different reason")
      _h.complete(false)
    end

actor \nodoc\ _TestSetTimerNotOpenSSLServer
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

class \nodoc\ iso _TestSetTimerNotOpenDuringSSLHandshakeServer is UnitTest
  """
  Test that set_timer returns SetTimerNotOpen during initial SSL handshake
  on the server side. An SSL server accepts a connection from a plain TCP
  client — the handshake stalls because the client doesn't speak TLS.
  """
  fun name(): String => "net/SetTimerNotOpenDuringSSLHandshakeServer"

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

    let listener = _TestSetTimerNotOpenSSLServerListener(consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(15_000_000_000)

actor \nodoc\ _TestSetTimerNotOpenSSLServerListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _sslctx: SSLContext val
  let _h: TestHelper
  var _client: (_TestSetTimerNotOpenSSLServerClient | None) = None
  let _servers: Array[_TestSetTimerNotOpenSSLServerConn] =
    Array[_TestSetTimerNotOpenSSLServerConn]
  let _timers: Timers = Timers

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9755",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSetTimerNotOpenSSLServerConn =>
    let server = _TestSetTimerNotOpenSSLServerConn(_sslctx, fd, _h, this)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSetTimerNotOpenSSLServerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSetTimerNotOpenSSLServerClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSetTimerNotOpenSSLServerListener")

  be _timer_checked() =>
    // The SSL server verified set_timer returns SetTimerNotOpen.
    // Now wait a bit then complete to let the connection clean up.
    let listener: _TestSetTimerNotOpenSSLServerListener tag = this
    let timer =
      Timer(
        _TestSetTimerNotOpenSSLServerWatchdog(listener),
        2_000_000_000,
        0)
    _timers(consume timer)

  be _watchdog_complete() =>
    _h.complete(true)

  be dispose() =>
    _timers.dispose()
    _tcp_listener.close()

class \nodoc\ _TestSetTimerNotOpenSSLServerWatchdog is TimerNotify
  let _listener: _TestSetTimerNotOpenSSLServerListener tag

  new iso create(listener: _TestSetTimerNotOpenSSLServerListener tag) =>
    _listener = listener

  fun ref apply(timer: Timer, count: U64): Bool =>
    _listener._watchdog_complete()
    false

actor \nodoc\ _TestSetTimerNotOpenSSLServerClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Plain TCP client (no SSL) — the SSL server's handshake will stall.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "9755",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

actor \nodoc\ _TestSetTimerNotOpenSSLServerConn
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSetTimerNotOpenSSLServerListener tag

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper,
    listener: _TestSetTimerNotOpenSSLServerListener tag)
  =>
    _h = h
    _listener = listener
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(_h.env.root),
        sslctx,
        fd,
        this,
        this)
    // Try to set a timer before _finish_initialization runs.
    // The state is still _ConnectionNone.
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let _: SetTimerNotOpen =>
        _listener._timer_checked()
      | let _: TimerToken =>
        _h.fail(
          "set_timer should return SetTimerNotOpen during SSL handshake")
        _h.complete(false)
      | let _: SetTimerAlreadyActive =>
        _h.fail(
          "set_timer should return SetTimerNotOpen, not SetTimerAlreadyActive")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _h.fail("SSL handshake should not complete against plain TCP client")
    _h.complete(false)

class \nodoc\ iso _TestFakeTimerSurvivesClose is UnitTest
  """
  An already-active timer fires after close() (graceful shutdown).

  Uses a fake backend whose receive always retries, so the connection stays
  in _Closing (no EOF to complete the close handshake). The timer fires from
  _Closing without any real TCP half-close to interfere.
  """
  fun name(): String => "net/FakeTimerSurvivesClose"

  fun apply(h: TestHelper) =>
    let a = _TestFakeTimerSurvivesCloseActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeTimerSurvivesCloseActor
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
    match \exhaustive\ MakeTimerDuration(1_000)
    | let d: TimerDuration =>
      match _tcp_connection.set_timer(d)
      | let _: SetTimerError =>
        _h.fail("set_timer returned error")
        _h.complete(false)
        return
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(1_000) should succeed")
      _h.complete(false)
      return
    end
    _tcp_connection.close()

  fun ref _on_timer(token: TimerToken) =>
    _h.complete(true)

  be dispose() =>
    _tcp_connection.hard_close()
