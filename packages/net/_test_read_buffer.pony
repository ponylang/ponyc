use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestReadBufferConstructorSize is UnitTest
  """
  Test that the constructor parameter sets the initial buffer size and minimum.
  The server verifies buffer behavior by resizing and checking invariants.
  """
  fun name(): String => "net/ReadBufferConstructorSize"

  fun apply(h: TestHelper) =>
    let listener = _TestReadBufferConstructorSizeListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestReadBufferConstructorSizeListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestReadBufferConstructorSizeServer] =
    Array[_TestReadBufferConstructorSizeServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7700", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestReadBufferConstructorSizeServer =>
    let server = _TestReadBufferConstructorSizeServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7700")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestReadBufferConstructorSizeServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    // Use a custom buffer size of 512
    match \exhaustive\ MakeReadBufferSize(512)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          TCPServerAuth(h.env.root), fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(512) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // set_read_buffer_minimum to 256 should succeed (lowering the minimum)
    match \exhaustive\ MakeReadBufferSize(256)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.set_read_buffer_minimum(rbs)
      | ReadBufferResized => None
      | ReadBufferResizeBelowBufferSize =>
        _h.fail("set_read_buffer_minimum(256) should succeed")
      end

      // resize_read_buffer to 256 should succeed since minimum is now 256
      match \exhaustive\ _tcp_connection.resize_read_buffer(rbs)
      | ReadBufferResized => None
      | let _: ReadBufferResizeBelowBufferSize =>
        _h.fail("resize_read_buffer(256) should succeed")
      | let _: ReadBufferResizeBelowUsed =>
        _h.fail("resize_read_buffer(256) should succeed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(256) should succeed")
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestSetReadBufferMinimumSuccess is UnitTest
  """
  Test that set_read_buffer_minimum() succeeds and grows the buffer when
  the new minimum exceeds the current allocation.
  """
  fun name(): String => "net/SetReadBufferMinimumSuccess"

  fun apply(h: TestHelper) =>
    let listener = _TestSetReadBufferMinSuccessListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestSetReadBufferMinSuccessListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestSetReadBufferMinSuccessServer] =
    Array[_TestSetReadBufferMinSuccessServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7701", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSetReadBufferMinSuccessServer =>
    let server = _TestSetReadBufferMinSuccessServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7701")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestSetReadBufferMinSuccessServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeReadBufferSize(256)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          TCPServerAuth(h.env.root), fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(256) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // Setting minimum to 512 should succeed and grow the buffer
    match \exhaustive\ MakeReadBufferSize(512)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.set_read_buffer_minimum(rbs)
      | ReadBufferResized => None
      | ReadBufferResizeBelowBufferSize =>
        _h.fail("set_read_buffer_minimum(512) should succeed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(512) should succeed")
    end

    // Setting minimum back to 128 should succeed (lowering is always ok)
    match \exhaustive\ MakeReadBufferSize(128)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.set_read_buffer_minimum(rbs)
      | ReadBufferResized => None
      | ReadBufferResizeBelowBufferSize =>
        _h.fail("set_read_buffer_minimum(128) should succeed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(128) should succeed")
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestSetReadBufferMinimumBelowBufferSize is UnitTest
  """
  Test that set_read_buffer_minimum() fails when the new minimum is below
  the current buffer-until value.
  """
  fun name(): String => "net/SetReadBufferMinimumBelowBufferSize"

  fun apply(h: TestHelper) =>
    let listener = _TestSetReadBufferMinBelowBufferSizeListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestSetReadBufferMinBelowBufferSizeListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestSetReadBufferMinBelowBufferSizeServer] =
    Array[_TestSetReadBufferMinBelowBufferSizeServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7702", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSetReadBufferMinBelowBufferSizeServer =>
    let server = _TestSetReadBufferMinBelowBufferSizeServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7702")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestSetReadBufferMinBelowBufferSizeServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(h.env.root), fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // Set buffer_until to 100
    match MakeBufferSize(100)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

    // Setting minimum below buffer_until should fail
    match \exhaustive\ MakeReadBufferSize(50)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.set_read_buffer_minimum(rbs)
      | ReadBufferResized =>
        _h.fail(
          "set_read_buffer_minimum(50) should fail when buffer_until is 100")
      | ReadBufferResizeBelowBufferSize => None
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(50) should succeed")
    end

    // Setting minimum at buffer_until should succeed
    match \exhaustive\ MakeReadBufferSize(100)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.set_read_buffer_minimum(rbs)
      | ReadBufferResized => None
      | ReadBufferResizeBelowBufferSize =>
        _h.fail(
          "set_read_buffer_minimum(100) should succeed when buffer_until is 100"
          )
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(100) should succeed")
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestResizeReadBufferSuccess is UnitTest
  """
  Test that resize_read_buffer() succeeds for valid sizes.
  """
  fun name(): String => "net/ResizeReadBufferSuccess"

  fun apply(h: TestHelper) =>
    let listener = _TestResizeReadBufferSuccessListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestResizeReadBufferSuccessListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestResizeReadBufferSuccessServer] =
    Array[_TestResizeReadBufferSuccessServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7703", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestResizeReadBufferSuccessServer =>
    let server = _TestResizeReadBufferSuccessServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7703")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestResizeReadBufferSuccessServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeReadBufferSize(1024)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          TCPServerAuth(h.env.root), fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(1024) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // Resize to larger
    match \exhaustive\ MakeReadBufferSize(4096)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.resize_read_buffer(rbs)
      | ReadBufferResized => None
      | let _: ReadBufferResizeBelowBufferSize =>
        _h.fail("resize_read_buffer(4096) should succeed")
      | let _: ReadBufferResizeBelowUsed =>
        _h.fail("resize_read_buffer(4096) should succeed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(4096) should succeed")
    end

    // Resize to smaller (also lowers minimum)
    match \exhaustive\ MakeReadBufferSize(512)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.resize_read_buffer(rbs)
      | ReadBufferResized => None
      | let _: ReadBufferResizeBelowBufferSize =>
        _h.fail("resize_read_buffer(512) should succeed")
      | let _: ReadBufferResizeBelowUsed =>
        _h.fail("resize_read_buffer(512) should succeed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(512) should succeed")
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestResizeReadBufferBelowBufferSize is UnitTest
  """
  Test that resize_read_buffer() fails when the size is below the current
  buffer-until value.
  """
  fun name(): String => "net/ResizeReadBufferBelowBufferSize"

  fun apply(h: TestHelper) =>
    let listener = _TestResizeReadBufferBelowBufferSizeListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestResizeReadBufferBelowBufferSizeListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestResizeReadBufferBelowBufferSizeServer] =
    Array[_TestResizeReadBufferBelowBufferSizeServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7704", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestResizeReadBufferBelowBufferSizeServer =>
    let server = _TestResizeReadBufferBelowBufferSizeServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7704")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestResizeReadBufferBelowBufferSizeServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(h.env.root), fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // Set buffer_until to 200
    match MakeBufferSize(200)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

    // Resize below buffer_until should fail
    match \exhaustive\ MakeReadBufferSize(100)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.resize_read_buffer(rbs)
      | ReadBufferResized =>
        _h.fail("resize_read_buffer(100) should fail when buffer_until is 200")
      | let _: ReadBufferResizeBelowBufferSize => None
      | let _: ReadBufferResizeBelowUsed =>
        _h.fail(
          "should be ReadBufferResizeBelowBufferSize, " +
          "not ReadBufferResizeBelowUsed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(100) should succeed")
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestResizeReadBufferBelowMinLowersMin is UnitTest
  """
  Test that resize_read_buffer() below the current minimum lowers the minimum.
  Verified by subsequently setting buffer_until to the old minimum (which would
  fail if the minimum hadn't been lowered).
  """
  fun name(): String => "net/ResizeReadBufferBelowMinLowersMin"

  fun apply(h: TestHelper) =>
    let listener = _TestResizeReadBufferBelowMinListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestResizeReadBufferBelowMinListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestResizeReadBufferBelowMinServer] =
    Array[_TestResizeReadBufferBelowMinServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7705", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestResizeReadBufferBelowMinServer =>
    let server = _TestResizeReadBufferBelowMinServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7705")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestResizeReadBufferBelowMinServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    // Start with buffer size 1024 (min is also 1024)
    match \exhaustive\ MakeReadBufferSize(1024)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          TCPServerAuth(h.env.root), fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(1024) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // Resize to 256 — this should lower the minimum from 1024 to 256
    match \exhaustive\ MakeReadBufferSize(256)
    | let rbs: ReadBufferSize =>
      match \exhaustive\ _tcp_connection.resize_read_buffer(rbs)
      | ReadBufferResized => None
      | let _: ReadBufferResizeBelowBufferSize =>
        _h.fail("resize_read_buffer(256) should succeed")
      | let _: ReadBufferResizeBelowUsed =>
        _h.fail("resize_read_buffer(256) should succeed")
      end
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(256) should succeed")
    end

    // Now buffer_until(512) should fail because minimum was lowered to 256
    match MakeBufferSize(512)
    | let e: BufferSize =>
      match \exhaustive\ _tcp_connection.buffer_until(e)
      | BufferUntilSet =>
        _h.fail("buffer_until(512) should fail when minimum is 256")
      | BufferSizeAboveMinimum => None
      end
    end

    // buffer_until(256) should succeed (at the new minimum)
    match MakeBufferSize(256)
    | let e: BufferSize =>
      match \exhaustive\ _tcp_connection.buffer_until(e)
      | BufferUntilSet => None
      | BufferSizeAboveMinimum =>
        _h.fail("buffer_until(256) should succeed when minimum is 256")
      end
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestBufferSizeAboveMinimum is UnitTest
  """
  Test that buffer_until() fails when the requested value exceeds the buffer
  minimum.
  """
  fun name(): String => "net/BufferSizeAboveMinimum"

  fun apply(h: TestHelper) =>
    let listener = _TestBufferSizeAboveMinListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestBufferSizeAboveMinListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestBufferSizeAboveMinServer] =
    Array[_TestBufferSizeAboveMinServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7706", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestBufferSizeAboveMinServer =>
    let server = _TestBufferSizeAboveMinServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7706")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestBufferSizeAboveMinServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    // Start with buffer size 128 (min is also 128)
    match \exhaustive\ MakeReadBufferSize(128)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          TCPServerAuth(h.env.root), fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(128) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // buffer_until(256) should fail because minimum is 128
    match MakeBufferSize(256)
    | let e: BufferSize =>
      match \exhaustive\ _tcp_connection.buffer_until(e)
      | BufferUntilSet =>
        _h.fail("buffer_until(256) should fail when minimum is 128")
      | BufferSizeAboveMinimum => None
      end
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestBufferSizeAtMinimum is UnitTest
  """
  Test that buffer_until() succeeds when the requested value equals the buffer
  minimum.
  """
  fun name(): String => "net/BufferSizeAtMinimum"

  fun apply(h: TestHelper) =>
    let listener = _TestBufferSizeAtMinListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestBufferSizeAtMinListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestReadBufferTriggerClient | None) = None
  let _servers: Array[_TestBufferSizeAtMinServer] =
    Array[_TestBufferSizeAtMinServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7707", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestBufferSizeAtMinServer =>
    let server = _TestBufferSizeAtMinServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestReadBufferTriggerClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7707")

  fun ref _on_closed() =>
    try (_client as _TestReadBufferTriggerClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestBufferSizeAtMinServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    match \exhaustive\ MakeReadBufferSize(256)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          TCPServerAuth(h.env.root), fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _h.fail("MakeReadBufferSize(256) should succeed")
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // buffer_until(256) should succeed (equals minimum)
    match MakeBufferSize(256)
    | let e: BufferSize =>
      match \exhaustive\ _tcp_connection.buffer_until(e)
      | BufferUntilSet => None
      | BufferSizeAboveMinimum =>
        _h.fail("buffer_until(256) should succeed when minimum is 256")
      end
    end

    _h.complete(true)
    _tcp_connection.close()

actor \nodoc\ _TestReadBufferTriggerClient is
  (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Minimal client that connects to trigger a server-side _on_accept, then
  closes. Used by read buffer tests that only need a server connection.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()

  new create(auth: TCPConnectAuth, host: String, port: String) =>
    _tcp_connection = TCPConnection.client(auth, host, port, "", this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.close()

  be dispose() =>
    _tcp_connection.close()
