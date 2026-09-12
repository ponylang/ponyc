use "constrained_types"
use "files"
use "pony_test"

class \nodoc\ iso _TestSendToken is UnitTest
  """
  Test that _on_send_accepted delivers a token for an accepted send() and
  that _on_sent fires with the matching token after data is handed to the OS.
  """
  fun name(): String => "net/SendToken"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("on_sent fired")

    let s = _TestSendTokenListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendTokenListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSendTokenClient | None) = None
  let _servers: Array[_TestSendTokenServer] = Array[_TestSendTokenServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7891",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendTokenServer =>
    let server = _TestSendTokenServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSendTokenClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSendTokenClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendTokenListener")

actor \nodoc\ _TestSendTokenClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7891",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _h.complete_action("client connected")
    match \exhaustive\ _tcp_connection.send("hello")
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

actor \nodoc\ _TestSendTokenServer
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

class \nodoc\ iso _TestSendAfterClose is UnitTest
  """
  Test that send() returns SendErrorNotConnected after the connection has been
  closed, and that a refused send fires no `_on_send_accepted`.
  """
  fun name(): String => "net/SendAfterClose"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("send error verified")

    let s = _TestSendAfterCloseListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendAfterCloseListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSendAfterCloseClient | None) = None
  let _servers: Array[_TestSendAfterCloseServer] =
    Array[_TestSendAfterCloseServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7892",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendAfterCloseServer =>
    let server = _TestSendAfterCloseServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSendAfterCloseClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSendAfterCloseClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendAfterCloseListener")

actor \nodoc\ _TestSendAfterCloseClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _accepted_count: USize = 0

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7892",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _h.complete_action("client connected")
    _tcp_connection.close()
    match \exhaustive\ _tcp_connection.send("should fail")
    | SendAccepted =>
      _h.fail("send() should have returned an error after close")
      _h.complete(false)
    | let _: SendErrorNotConnected =>
      _h.assert_eq[USize](
        0,
        _accepted_count,
        "a refused send must not fire _on_send_accepted")
      _h.complete_action("send error verified")
    | let _: SendError =>
      _h.fail("send() returned wrong error type after close")
      _h.complete(false)
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted_count = _accepted_count + 1

actor \nodoc\ _TestSendAfterCloseServer
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

class \nodoc\ iso _TestSendv is UnitTest
  """
  Test that send() with multiple buffers delivers them as a single contiguous
  stream and that _on_sent fires with the matching token.
  """
  fun name(): String => "net/Sendv"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("data verified")
    h.expect_action("on_sent fired")

    let s = _TestSendvListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendvListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSendvClient | None) = None
  let _servers: Array[_TestSendvServer] = Array[_TestSendvServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7893",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendvServer =>
    let server = _TestSendvServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSendvClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSendvClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendvListener")

actor \nodoc\ _TestSendvClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7893",
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
      recover val [as ByteSeq: "Hello"; ", "; "world!"] end)
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

actor \nodoc\ _TestSendvServer
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
    match MakeBufferSize(13)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.assert_eq[String]("Hello, world!", String.from_array(consume data))
    _h.complete_action("data verified")
    _tcp_connection.close()
    KeepReading

class \nodoc\ iso _TestSendvEmpty is UnitTest
  """
  Test that send() with an empty ByteSeqIter is accepted and that _on_sent
  fires.
  """
  fun name(): String => "net/SendvEmpty"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("on_sent fired")

    let s = _TestSendvEmptyListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendvEmptyListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSendvEmptyClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7894",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSendvEmptyClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSendvEmptyClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendvEmptyListener")

actor \nodoc\ _TestSendvEmptyClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7894",
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
      recover val Array[ByteSeq] end)
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send() returned an error for empty array")
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

class \nodoc\ iso _TestSendvMixedEmpty is UnitTest
  """
  Test that send() with multiple buffers correctly skips empty buffers.
  Sends ["Hello"; ""; "world"] and verifies the server receives "Helloworld".
  """
  fun name(): String => "net/SendvMixedEmpty"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("data verified")

    let s = _TestSendvMixedEmptyListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendvMixedEmptyListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSendvMixedEmptyClient | None) = None
  let _servers: Array[_TestSendvMixedEmptyServer] =
    Array[_TestSendvMixedEmptyServer]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7895",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendvMixedEmptyServer =>
    let server = _TestSendvMixedEmptyServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestSendvMixedEmptyClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSendvMixedEmptyClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendvMixedEmptyListener")

actor \nodoc\ _TestSendvMixedEmptyClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7895",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _h.complete_action("client connected")
    _tcp_connection.send(
      recover val [as ByteSeq: "Hello"; ""; "world"] end)

actor \nodoc\ _TestSendvMixedEmptyServer
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
    match MakeBufferSize(10)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.assert_eq[String]("Helloworld", String.from_array(consume data))
    _h.complete_action("data verified")
    _tcp_connection.close()
    KeepReading

class \nodoc\ iso _TestSendPerTokenCompletion is UnitTest
  """
  Overlapping sends each get exactly one `_on_sent`, in send order.

  The client mutes with a tiny SO_RCVBUF so the pipe fills. The server floods
  64 KiB chunks until `_on_throttled` fires, then sends one more chunk from
  `_on_unthrottled` so two sends are pending at once. The client then drains
  everything.

  Asserts every accepted token fires `_on_sent` exactly once, in ascending id
  order (proving none was lost or reordered). A single shared pending token
  would let the second pending send overwrite the first, so an earlier token
  never fires `_on_sent`: the ordering assertion sees a later id where it
  expects the earlier one.

  POSIX only -- Windows loopback ignores SO_SNDBUF/SO_RCVBUF.
  """
  fun name(): String => "net/SendPerTokenCompletion"

  fun ref apply(h: TestHelper) =>
    let listener = _TestSendPerTokenListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendPerTokenListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendPerTokenServer] = Array[_TestSendPerTokenServer]
  var _client: (_TestSendPerTokenClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7910",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendPerTokenServer =>
    let s = _TestSendPerTokenServer(fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendPerTokenClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendPerTokenClient).dispose() end
    for server in _servers.values() do server.dispose() end

  be unmute_client() =>
    try (_client as _TestSendPerTokenClient).resume_reading() end

actor \nodoc\ _TestSendPerTokenClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7910",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    // Small receive buffer + muted reads so the sender's pipe fills fast.
    // BSDs need a larger buffer to avoid TCP flow control stalls amplified
    // by kqueue wakeup delay.
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    None
    KeepReading

actor \nodoc\ _TestSendPerTokenServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendPerTokenListener
  var _total_sends: USize = 0
  var _throttled: Bool = false
  var _started: Bool = false
  var _unmuted_client: Bool = false
  var _resumed: Bool = false
  var _all_sends_done: Bool = false
  embed _accepted: Array[USize] = _accepted.create()
  embed _completed: Array[USize] = _completed.create()
  var _expected_next: USize = 1

  new create(fd: U32, h: TestHelper, listener: _TestSendPerTokenListener) =>
    _h = h
    _listener = listener
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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted.push(token.id)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => _total_sends = _total_sends + 1
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end

  fun ref _on_unthrottled() =>
    if not _resumed then
      _resumed = true
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => _total_sends = _total_sends + 1
      | let _: SendError => None
      end
      _all_sends_done = true
      _check_completion()
    end

  fun ref _on_sent(token: SendToken) =>
    _h.assert_eq[USize](
      _expected_next,
      token.id,
      "_on_sent fired out of order or with a gap")
    _expected_next = _expected_next + 1
    _completed.push(token.id)
    _check_completion()

  fun ref _check_completion() =>
    if not _all_sends_done then return end
    if _completed.size() == _total_sends then
      _h.assert_eq[USize](
        _accepted.size(),
        _completed.size(),
        "every accepted token must fire _on_sent exactly once")
      _tcp_connection.close()
      _h.complete(true)
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("no send should fail in the drain scenario")

class \nodoc\ iso _TestSendSSLLargeSingleSend is UnitTest
  """
  A single large SSL send is delivered in full across multiple writeable
  events.

  The client makes one 256 KiB SSL send and nothing after it; the server
  echoes each decrypted chunk. A tiny SO_RCVBUF on both ends forces the
  ciphertext to partial-write, so it can only reach the wire in pieces on
  successive writeable events. The echo returns all 256 KiB only if the
  pending-write drain loop keeps flushing that one send's queued ciphertext
  as the socket clears, so the test exercises multi-write drain of a single
  SSL send.
  """
  fun name(): String => "net/SendSSLLargeSingleSend"

  fun apply(h: TestHelper) ? =>
    let port = "7911"
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

    let listener = _TestSendSSLLargeSingleSendListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendSSLLargeSingleSendListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSendSSLLargeSingleSendClient | None) = None
  let _servers: Array[_TestSendSSLLargeSingleSendServer] =
    Array[_TestSendSSLLargeSingleSendServer]

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

  fun ref _on_accept(fd: U32): _TestSendSSLLargeSingleSendServer =>
    let s = _TestSendSSLLargeSingleSendServer(_sslctx, fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendSSLLargeSingleSendClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendSSLLargeSingleSendClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendSSLLargeSingleSendListener")

actor \nodoc\ _TestSendSSLLargeSingleSendClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _expected: USize = 262144
  var _received: USize = 0
  var _corrupt: Bool = false

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
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    match \exhaustive\ _tcp_connection.send(
      recover val Array[U8].init('x', _expected) end)
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("client send failed")
      _h.complete(false)
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let d: Array[U8] val = consume data
    for b in d.values() do
      if b != 'x' then _corrupt = true end
    end
    _received = _received + d.size()
    if _received == _expected then
      _h.assert_false(_corrupt, "echoed bytes must all be 'x'")
      _tcp_connection.close()
      _h.complete(true)
    elseif _received > _expected then
      _h.fail("received more bytes than were sent")
      _h.complete(false)
    end
    KeepReading

actor \nodoc\ _TestSendSSLLargeSingleSendServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _pending: Array[Array[U8] val] = Array[Array[U8] val]

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
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let d: Array[U8] val = consume data
    if _pending.size() > 0 then
      _pending.push(d)
    else
      match \exhaustive\ _tcp_connection.send(d)
      | SendAccepted => None
      | let _: SendError => _pending.push(d)
      end
    end
    KeepReading

  fun ref _on_unthrottled() =>
    _drain_pending()

  fun ref _drain_pending() =>
    while _pending.size() > 0 do
      try
        let d = _pending(0)?
        match \exhaustive\ _tcp_connection.send(d)
        | SendAccepted => _pending.shift()?
        | let _: SendError => return
        end
      end
    end

class \nodoc\ iso _TestSendMidFlightDropBoundary is UnitTest
  """
  On a mid-flight drop, the `_on_sent` / `_on_send_failed` split is a clean
  boundary: every accepted send fires exactly one of the two, in a prefix of
  `_on_sent` (bytes that reached the OS) followed by a suffix of
  `_on_send_failed` (bytes that did not).

  The server floods 64 KiB chunks until `_on_throttled` fires, then floods
  again from `_on_unthrottled` and immediately `hard_close()`s, leaving
  at least one send pending.

  Asserts each token fires exactly one terminal callback, that at least one
  fires `_on_send_failed`, that `max(_on_sent id) < min(_on_send_failed id)`,
  and that `_on_send_failed` arrives after `_on_closed`.

  POSIX only -- Windows loopback ignores SO_SNDBUF/SO_RCVBUF.
  """
  fun name(): String => "net/SendMidFlightDropBoundary"

  fun ref apply(h: TestHelper) =>
    let listener = _TestSendMidFlightDropListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendMidFlightDropListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendMidFlightDropServer] =
    Array[_TestSendMidFlightDropServer]
  var _client: (_TestSendMidFlightDropClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7912",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendMidFlightDropServer =>
    let s = _TestSendMidFlightDropServer(fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendMidFlightDropClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendMidFlightDropClient).dispose() end
    for server in _servers.values() do server.dispose() end

  be unmute_client() =>
    try (_client as _TestSendMidFlightDropClient).resume_reading() end

actor \nodoc\ _TestSendMidFlightDropClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7912",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    None
    KeepReading

actor \nodoc\ _TestSendMidFlightDropServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendMidFlightDropListener
  var _total_sends: USize = 0
  var _throttled: Bool = false
  var _started: Bool = false
  var _unmuted_client: Bool = false
  var _resumed: Bool = false
  var _closed: Bool = false
  var _all_sends_done: Bool = false
  embed _accepted: Array[USize] = _accepted.create()
  embed _sent_ids: Array[USize] = _sent_ids.create()
  embed _failed_ids: Array[USize] = _failed_ids.create()

  new create(fd: U32,
    h: TestHelper,
    listener: _TestSendMidFlightDropListener)
  =>
    _h = h
    _listener = listener
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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted.push(token.id)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => _total_sends = _total_sends + 1
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end

  fun ref _on_unthrottled() =>
    if not _resumed then
      _resumed = true
      _throttled = false
      while not _throttled do
        let chunk = recover val Array[U8].init('x', 65536) end
        match \exhaustive\ _tcp_connection.send(chunk)
        | SendAccepted => _total_sends = _total_sends + 1
        | let _: SendError => break
        end
      end
      _all_sends_done = true
      _tcp_connection.hard_close()
    end

  fun ref _on_closed() =>
    _closed = true

  fun ref _on_sent(token: SendToken) =>
    _sent_ids.push(token.id)
    _maybe_finish()

  fun ref _on_send_failed(token: SendToken) =>
    _h.assert_true(_closed, "_on_send_failed must arrive after _on_closed")
    _failed_ids.push(token.id)
    _maybe_finish()

  fun ref _maybe_finish() =>
    if not _all_sends_done then return end
    if (_sent_ids.size() + _failed_ids.size()) != _total_sends then
      return
    end

    for id in _accepted.values() do
      var count: USize = 0
      for s in _sent_ids.values() do
        if s == id then count = count + 1 end
      end
      for f in _failed_ids.values() do
        if f == id then count = count + 1 end
      end
      _h.assert_eq[USize](
        1,
        count,
        "each token must fire exactly one terminal callback")
    end

    _h.assert_true(
      _failed_ids.size() >= 1,
      "at least one accepted-but-undelivered send must fire _on_send_failed")

    var max_sent: USize = 0
    for s in _sent_ids.values() do
      if s > max_sent then max_sent = s end
    end
    var min_failed: USize = USize.max_value()
    for f in _failed_ids.values() do
      if f < min_failed then min_failed = f end
    end
    _h.assert_true(
      max_sent < min_failed,
      "_on_sent ids must all precede _on_send_failed ids")

    var prev_failed: USize = 0
    for f in _failed_ids.values() do
      _h.assert_true(
        f > prev_failed,
        "_on_send_failed must fire in ascending (send) order")
      prev_failed = f
    end

    _h.complete(true)

class \nodoc\ iso _TestSendSSLPerTokenCompletion is UnitTest
  """
  Overlapping SSL sends each get exactly one `_on_sent`, in send order.

  The SSL analogue of `SendPerTokenCompletion`. An SSL send's completion
  offset is captured after its ciphertext is enqueued, so this checks the
  per-token FIFO fires correctly when several encrypted sends are queued at
  once. The client mutes with a tiny SO_RCVBUF so the sender's pipe fills.
  The server floods 64 KiB chunks until `_on_throttled` fires, so
  backpressure triggers regardless of how the kernel rounds buffer hints.
  When the pipe drains enough to unthrottle, the server makes one more send
  from `_on_unthrottled`, so at least two tokens are pending at once. The
  client then drains everything.

  Asserts every accepted token fires `_on_sent` exactly once, in ascending id
  order.

  POSIX only, for the same reason as `SendPerTokenCompletion`.
  """
  fun name(): String => "net/SendSSLPerTokenCompletion"

  fun apply(h: TestHelper) ? =>
    let port = "7913"
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

    let listener = _TestSendSSLPerTokenListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendSSLPerTokenListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendSSLPerTokenServer] =
    Array[_TestSendSSLPerTokenServer]
  var _client: (_TestSendSSLPerTokenClient | None) = None

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

  fun ref _on_accept(fd: U32): _TestSendSSLPerTokenServer =>
    let s = _TestSendSSLPerTokenServer(_sslctx, fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendSSLPerTokenClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendSSLPerTokenClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendSSLPerTokenListener")

  be unmute_client() =>
    try (_client as _TestSendSSLPerTokenClient).resume_reading() end

actor \nodoc\ _TestSendSSLPerTokenClient
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

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    None
    KeepReading

actor \nodoc\ _TestSendSSLPerTokenServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendSSLPerTokenListener
  var _total_sends: USize = 0
  var _started: Bool = false
  var _throttled: Bool = false
  var _unmuted_client: Bool = false
  var _resumed: Bool = false
  var _all_sends_done: Bool = false
  embed _accepted: Array[USize] = _accepted.create()
  embed _completed: Array[USize] = _completed.create()
  var _expected_next: USize = 1

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper,
    listener: _TestSendSSLPerTokenListener)
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

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted.push(token.id)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => _total_sends = _total_sends + 1
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end

  fun ref _on_unthrottled() =>
    if not _resumed then
      _resumed = true
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => _total_sends = _total_sends + 1
      | let _: SendError => None
      end
      _all_sends_done = true
      _check_completion()
    end

  fun ref _on_sent(token: SendToken) =>
    _h.assert_eq[USize](
      _expected_next,
      token.id,
      "_on_sent fired out of order or with a gap")
    _expected_next = _expected_next + 1
    _completed.push(token.id)
    _check_completion()

  fun ref _check_completion() =>
    if not _all_sends_done then return end
    if _completed.size() == _total_sends then
      _h.assert_eq[USize](
        _accepted.size(),
        _completed.size(),
        "every accepted token must fire _on_sent exactly once")
      _tcp_connection.close()
      _h.complete(true)
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("no send should fail in the drain scenario")

class \nodoc\ iso _TestSendSSLMidFlightDropBoundary is UnitTest
  """
  The SSL analogue of `SendMidFlightDropBoundary`. On a mid-flight drop of an
  SSL connection, the `_on_sent` / `_on_send_failed` split is still a clean
  boundary: every accepted send fires exactly one of the two, a prefix of
  `_on_sent` followed by a suffix of `_on_send_failed`.

  Same backpressure setup as `SendSSLPerTokenCompletion`: the server floods
  64 KiB chunks until `_on_throttled` fires, then floods again from
  `_on_unthrottled` and immediately `hard_close()`s. At least one token is
  pending when the connection drops.

  Asserts each token fires exactly one terminal callback, that at least one
  fires `_on_send_failed`, that `max(_on_sent id) < min(_on_send_failed id)`,
  and that `_on_send_failed` arrives after `_on_closed`.

  POSIX only, for the same reason as `SendMidFlightDropBoundary`.
  """
  fun name(): String => "net/SendSSLMidFlightDropBoundary"

  fun apply(h: TestHelper) ? =>
    let port = "7914"
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

    let listener = _TestSendSSLMidFlightDropListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendSSLMidFlightDropListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendSSLMidFlightDropServer] =
    Array[_TestSendSSLMidFlightDropServer]
  var _client: (_TestSendSSLMidFlightDropClient | None) = None

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

  fun ref _on_accept(fd: U32): _TestSendSSLMidFlightDropServer =>
    let s = _TestSendSSLMidFlightDropServer(_sslctx, fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendSSLMidFlightDropClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendSSLMidFlightDropClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendSSLMidFlightDropListener")

  be unmute_client() =>
    try (_client as _TestSendSSLMidFlightDropClient).resume_reading() end

actor \nodoc\ _TestSendSSLMidFlightDropClient
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

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    None
    KeepReading

actor \nodoc\ _TestSendSSLMidFlightDropServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendSSLMidFlightDropListener
  var _total_sends: USize = 0
  var _started: Bool = false
  var _throttled: Bool = false
  var _unmuted_client: Bool = false
  var _resumed: Bool = false
  var _closed: Bool = false
  var _all_sends_done: Bool = false
  embed _accepted: Array[USize] = _accepted.create()
  embed _sent_ids: Array[USize] = _sent_ids.create()
  embed _failed_ids: Array[USize] = _failed_ids.create()

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper,
    listener: _TestSendSSLMidFlightDropListener)
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

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted.push(token.id)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => _total_sends = _total_sends + 1
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end

  fun ref _on_unthrottled() =>
    if not _resumed then
      _resumed = true
      _throttled = false
      while not _throttled do
        let chunk = recover val Array[U8].init('x', 65536) end
        match \exhaustive\ _tcp_connection.send(chunk)
        | SendAccepted => _total_sends = _total_sends + 1
        | let _: SendError => break
        end
      end
      _all_sends_done = true
      _tcp_connection.hard_close()
    end

  fun ref _on_closed() =>
    _closed = true

  fun ref _on_sent(token: SendToken) =>
    _sent_ids.push(token.id)
    _maybe_finish()

  fun ref _on_send_failed(token: SendToken) =>
    _h.assert_true(_closed, "_on_send_failed must arrive after _on_closed")
    _failed_ids.push(token.id)
    _maybe_finish()

  fun ref _maybe_finish() =>
    if not _all_sends_done then return end
    if (_sent_ids.size() + _failed_ids.size()) != _total_sends then
      return
    end

    for id in _accepted.values() do
      var count: USize = 0
      for s in _sent_ids.values() do
        if s == id then count = count + 1 end
      end
      for f in _failed_ids.values() do
        if f == id then count = count + 1 end
      end
      _h.assert_eq[USize](
        1,
        count,
        "each token must fire exactly one terminal callback")
    end

    _h.assert_true(
      _failed_ids.size() >= 1,
      "at least one accepted-but-undelivered send must fire _on_send_failed")

    var max_sent: USize = 0
    for s in _sent_ids.values() do
      if s > max_sent then max_sent = s end
    end
    var min_failed: USize = USize.max_value()
    for f in _failed_ids.values() do
      if f < min_failed then min_failed = f end
    end
    _h.assert_true(
      max_sent < min_failed,
      "_on_sent ids must all precede _on_send_failed ids")

    var prev_failed: USize = 0
    for f in _failed_ids.values() do
      _h.assert_true(
        f > prev_failed,
        "_on_send_failed must fire in ascending (send) order")
      prev_failed = f
    end

    _h.complete(true)

class \nodoc\ iso _TestSendGracefulCloseWithPending is UnitTest
  """
  A graceful close() with sends still queued under backpressure flushes those
  queued writes to the peer before shutting down, rather than dropping them.

  The server floods 64 KiB chunks until `_on_throttled` fires, then sends one
  more chunk from `_on_unthrottled` and calls close(). Because close() is
  graceful, every queued byte is written before FIN: all tokens fire `_on_sent`
  (none fire `_on_send_failed`), the client receives every byte the server
  sent, and `_on_closed` fires.

  The three completion signals fire at different times -- the last `_on_sent`
  lands as the queue drains, before the server's `_on_closed` (which waits on
  the peer's FIN) -- so each is its own expected action rather than one joint
  check.

  POSIX only -- Windows loopback ignores SO_SNDBUF/SO_RCVBUF.
  """
  fun name(): String => "net/SendGracefulCloseWithPending"

  fun ref apply(h: TestHelper) =>
    h.expect_action("server delivered all pending")
    h.expect_action("server closed")
    h.expect_action("client received all bytes")

    let listener = _TestSendGracefulCloseListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendGracefulCloseListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendGracefulCloseServer] =
    Array[_TestSendGracefulCloseServer]
  var _client: (_TestSendGracefulCloseClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7915",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendGracefulCloseServer =>
    let s = _TestSendGracefulCloseServer(fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendGracefulCloseClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendGracefulCloseClient).dispose() end
    for server in _servers.values() do server.dispose() end

  be unmute_client() =>
    try (_client as _TestSendGracefulCloseClient).resume_reading() end

  be server_total(bytes: USize) =>
    try (_client as _TestSendGracefulCloseClient).set_expected(bytes) end

actor \nodoc\ _TestSendGracefulCloseClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected: USize = 0
  var _total_received: USize = 0
  var _signalled: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7915",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  be set_expected(n: USize) =>
    _expected = n
    _check_received()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _total_received = _total_received + data.size()
    _check_received()
    KeepReading

  fun ref _check_received() =>
    if (not _signalled) and (_expected > 0) and
      (_total_received >= _expected)
    then
      _signalled = true
      _h.complete_action("client received all bytes")
    end

actor \nodoc\ _TestSendGracefulCloseServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendGracefulCloseListener
  var _total_sends: USize = 0
  var _total_bytes: USize = 0
  var _throttled: Bool = false
  var _started: Bool = false
  var _unmuted_client: Bool = false
  var _resumed: Bool = false
  var _all_sends_done: Bool = false
  embed _accepted: Array[USize] = _accepted.create()
  embed _sent_ids: Array[USize] = _sent_ids.create()

  new create(fd: U32,
    h: TestHelper,
    listener: _TestSendGracefulCloseListener)
  =>
    _h = h
    _listener = listener
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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted.push(token.id)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted =>
        _total_sends = _total_sends + 1
        _total_bytes = _total_bytes + 65536
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end

  fun ref _on_unthrottled() =>
    if not _resumed then
      _resumed = true
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted =>
        _total_sends = _total_sends + 1
        _total_bytes = _total_bytes + 65536
      | let _: SendError => None
      end
      _all_sends_done = true
      _tcp_connection.close()
      _listener.server_total(_total_bytes)
      _check_completion()
    end

  fun ref _on_closed() =>
    _h.complete_action("server closed")

  fun ref _on_sent(token: SendToken) =>
    _sent_ids.push(token.id)
    _check_completion()

  fun ref _check_completion() =>
    if not _all_sends_done then return end
    if _sent_ids.size() == _total_sends then
      for id in _accepted.values() do
        var count: USize = 0
        for s in _sent_ids.values() do
          if s == id then count = count + 1 end
        end
        _h.assert_eq[USize](
          1,
          count,
          "each token must fire _on_sent exactly once")
      end
      _h.complete_action("server delivered all pending")
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("graceful close must flush queued writes, not fail them")

class \nodoc\ iso _TestSendSSLGracefulCloseWithPending is UnitTest
  """
  The SSL analogue of `SendGracefulCloseWithPending`. A graceful close() on an
  SSL connection with sends still queued under backpressure flushes those
  queued writes (ciphertext) to the peer before shutting down.

  The server floods 64 KiB chunks until `_on_throttled` fires, then makes one
  more send from `_on_unthrottled` and calls close(). Because close() is
  graceful, every queued byte is written before FIN: all tokens fire `_on_sent`
  (none fire `_on_send_failed`), the client decrypts every byte the server
  sent, and `_on_closed` fires. The SSL path is not a relabel of the plaintext
  one: the queued bytes are ciphertext, and the `_Closing` drain runs
  `_ssl_flush_sends`, which can enqueue more ciphertext and re-defer the FIN.

  POSIX only, for the same reason as `SendSSLPerTokenCompletion`.
  """
  fun name(): String => "net/SendSSLGracefulCloseWithPending"

  fun apply(h: TestHelper) ? =>
    let port = "7916"
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

    h.expect_action("server delivered all pending")
    h.expect_action("server closed")
    h.expect_action("client received all bytes")

    let listener = _TestSendSSLGracefulCloseListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendSSLGracefulCloseListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendSSLGracefulCloseServer] =
    Array[_TestSendSSLGracefulCloseServer]
  var _client: (_TestSendSSLGracefulCloseClient | None) = None

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

  fun ref _on_accept(fd: U32): _TestSendSSLGracefulCloseServer =>
    let s = _TestSendSSLGracefulCloseServer(_sslctx, fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendSSLGracefulCloseClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendSSLGracefulCloseClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendSSLGracefulCloseListener")

  be unmute_client() =>
    try (_client as _TestSendSSLGracefulCloseClient).resume_reading() end

  be server_total(bytes: USize) =>
    try (_client as _TestSendSSLGracefulCloseClient).set_expected(bytes) end

actor \nodoc\ _TestSendSSLGracefulCloseClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected: USize = 0
  var _total_received: USize = 0
  var _signalled: Bool = false

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

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  be set_expected(n: USize) =>
    _expected = n
    _check_received()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _total_received = _total_received + data.size()
    _check_received()
    KeepReading

  fun ref _check_received() =>
    if (not _signalled) and (_expected > 0) and
      (_total_received >= _expected)
    then
      _signalled = true
      _h.complete_action("client received all bytes")
    end

actor \nodoc\ _TestSendSSLGracefulCloseServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendSSLGracefulCloseListener
  var _total_sends: USize = 0
  var _total_bytes: USize = 0
  var _started: Bool = false
  var _throttled: Bool = false
  var _unmuted_client: Bool = false
  var _resumed: Bool = false
  var _all_sends_done: Bool = false
  embed _accepted: Array[USize] = _accepted.create()
  embed _sent_ids: Array[USize] = _sent_ids.create()

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper,
    listener: _TestSendSSLGracefulCloseListener)
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

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted.push(token.id)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted =>
        _total_sends = _total_sends + 1
        _total_bytes = _total_bytes + 65536
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end

  fun ref _on_unthrottled() =>
    if not _resumed then
      _resumed = true
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted =>
        _total_sends = _total_sends + 1
        _total_bytes = _total_bytes + 65536
      | let _: SendError => None
      end
      _all_sends_done = true
      _tcp_connection.close()
      _listener.server_total(_total_bytes)
      _check_completion()
    end

  fun ref _on_closed() =>
    _h.complete_action("server closed")

  fun ref _on_sent(token: SendToken) =>
    _sent_ids.push(token.id)
    _check_completion()

  fun ref _check_completion() =>
    if not _all_sends_done then return end
    if _sent_ids.size() == _total_sends then
      for id in _accepted.values() do
        var count: USize = 0
        for s in _sent_ids.values() do
          if s == id then count = count + 1 end
        end
        _h.assert_eq[USize](
          1,
          count,
          "each token must fire _on_sent exactly once")
      end
      _h.complete_action("server delivered all pending")
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("graceful close must flush queued writes, not fail them")

class \nodoc\ iso _TestSendCloseFromThrottled is UnitTest
  """
  A `send()` writes to the socket before it returns. A partial write applies
  backpressure and runs `_on_throttled` right there, inside the `send()` call,
  so an application that closes from `_on_throttled` closes the connection from
  inside a `send()` that has already put bytes on the wire.

  The sends are accepted: `_on_send_accepted` delivers their tokens and
  `send()` returns `SendAccepted`, and because a graceful close flushes what
  is queued, every token fires `_on_sent` once `_Closing` drains.

  The server floods 64 KiB chunks in a loop until `_on_throttled` fires, so
  the test triggers backpressure regardless of how much the kernel rounds
  `SO_SNDBUF`/`SO_RCVBUF` up.

  POSIX only, for the same reason as `SendPerTokenCompletion`.
  """
  fun name(): String => "net/SendCloseFromThrottled"

  fun ref apply(h: TestHelper) =>
    h.expect_action("send accepted")
    h.expect_action("token sent")
    h.expect_action("client received all bytes")

    let listener = _TestSendCloseFromThrottledListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendCloseFromThrottledListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendCloseFromThrottledServer] =
    Array[_TestSendCloseFromThrottledServer]
  var _client: (_TestSendCloseFromThrottledClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9781",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendCloseFromThrottledServer =>
    let s = _TestSendCloseFromThrottledServer(fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendCloseFromThrottledClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendCloseFromThrottledClient).dispose() end
    for server in _servers.values() do server.dispose() end

  be unmute_client() =>
    try (_client as _TestSendCloseFromThrottledClient).resume_reading() end

  be server_payload_size(n: USize) =>
    try (_client as _TestSendCloseFromThrottledClient).set_expected(n) end

actor \nodoc\ _TestSendCloseFromThrottledClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _expected: USize = 0
  var _total_received: USize = 0
  var _signalled: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9781",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  be set_expected(n: USize) =>
    _expected = n
    _check_received()

  fun ref _check_received() =>
    if (not _signalled) and (_expected > 0) and
      (_total_received >= _expected)
    then
      _signalled = true
      _h.complete_action("client received all bytes")
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _total_received = _total_received + data.size()
    _check_received()
    KeepReading

actor \nodoc\ _TestSendCloseFromThrottledServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendCloseFromThrottledListener
  var _started: Bool = false
  var _throttled: Bool = false
  var _unmuted_client: Bool = false
  var _total_sends: USize = 0
  var _total_sent: USize = 0

  new create(fd: U32,
    h: TestHelper,
    listener: _TestSendCloseFromThrottledListener)
  =>
    _h = h
    _listener = listener
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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    var payload_size: USize = 0
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => payload_size = payload_size + 65536
      | let _: SendError => break
      end
    end
    _h.complete_action("send accepted")
    _listener.server_payload_size(payload_size)
    KeepReading

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _total_sends = _total_sends + 1

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end
    _tcp_connection.close()

  fun ref _on_sent(token: SendToken) =>
    _total_sent = _total_sent + 1
    if _total_sent == _total_sends then
      _h.complete_action("token sent")
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("a graceful close flushes queued writes, so this send must not " +
      "fail")

class \nodoc\ iso _TestSendHardCloseFromThrottled is UnitTest
  """
  The hard-close twin of `SendCloseFromThrottled`.

  `_on_throttled` runs inside a `send()` that hit backpressure. The
  application calls `hard_close()`, which drops the queued remainder. The
  sends are accepted -- `_on_send_accepted` delivers tokens -- and those
  tokens fire `_on_send_failed`, because the sends never finished reaching
  the OS.

  The server floods 64 KiB chunks in a loop until `_on_throttled` fires, so
  the test triggers backpressure regardless of how much the kernel rounds
  `SO_SNDBUF`/`SO_RCVBUF` up.

  This is the negative case, and it is why `SendCloseFromThrottled` is not
  enough on its own. That test asserts tokens come back and reach `_on_sent`,
  which a `_do_send` that reported every send as delivered would satisfy too.
  This one drives the same setup to the opposite outcome, so a `_do_send`
  that mints a token without tracking whether its bytes reached the OS fails
  one of the two.

  POSIX only, for the same reason as `SendPerTokenCompletion`.
  """
  fun name(): String => "net/SendHardCloseFromThrottled"

  fun ref apply(h: TestHelper) =>
    h.expect_action("send accepted")
    h.expect_action("token failed")
    h.expect_action("server closed")

    let listener = _TestSendHardCloseFromThrottledListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendHardCloseFromThrottledListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendHardCloseFromThrottledServer] =
    Array[_TestSendHardCloseFromThrottledServer]
  var _client: (_TestSendHardCloseFromThrottledClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9782",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendHardCloseFromThrottledServer =>
    let s = _TestSendHardCloseFromThrottledServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendHardCloseFromThrottledClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendHardCloseFromThrottledClient).dispose() end
    for server in _servers.values() do server.dispose() end

actor \nodoc\ _TestSendHardCloseFromThrottledClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9782",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

actor \nodoc\ _TestSendHardCloseFromThrottledServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _started: Bool = false
  var _throttled: Bool = false
  var _closed: Bool = false
  var _failed_signalled: Bool = false

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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => None
      | let _: SendError => break
      end
    end
    _h.assert_true(
      _closed,
      "_on_closed must fire before the flood loop exits")
    _h.complete_action("send accepted")
    KeepReading

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    None

  fun ref _on_throttled() =>
    _throttled = true
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _closed = true
    _h.complete_action("server closed")

  fun ref _on_sent(token: SendToken) =>
    _h.fail("a hard close drops the queued remainder, so this send must not " +
      "report as sent")

  fun ref _on_send_failed(token: SendToken) =>
    if not _failed_signalled then
      _failed_signalled = true
      _h.complete_action("token failed")
    end

class \nodoc\ iso _TestSendSSLHardCloseFromThrottled is UnitTest
  """
  The SSL analogue of `SendHardCloseFromThrottled`. The application calls
  `hard_close()` from the `_on_throttled` that runs inside its own `send()`.
  `_on_send_accepted` delivers tokens and those tokens fire
  `_on_send_failed`.

  The server floods 64 KiB chunks in a loop until `_on_throttled` fires, so
  the test triggers backpressure regardless of how much the kernel rounds
  `SO_SNDBUF`/`SO_RCVBUF` up.

  The SSL path is not a relabel of the plaintext one. `_do_send` encrypts the
  payload into the SSL session and enqueues the ciphertext before it mints the
  token, so the token's completion offset is over ciphertext bytes, not the
  application's. This checks that a send accounted in ciphertext still gets its
  one callback when a hard close inside the flush discards the queue.

  POSIX only, for the same reason as `SendSSLPerTokenCompletion`.
  """
  fun name(): String => "net/SendSSLHardCloseFromThrottled"

  fun apply(h: TestHelper) ? =>
    let port = "9783"
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
    h.expect_action("token failed")
    h.expect_action("server closed")

    let listener =
      _TestSendSSLHardCloseFromThrottledListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendSSLHardCloseFromThrottledListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendSSLHardCloseFromThrottledServer] =
    Array[_TestSendSSLHardCloseFromThrottledServer]
  var _client: (_TestSendSSLHardCloseFromThrottledClient | None) = None

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

  fun ref _on_accept(fd: U32): _TestSendSSLHardCloseFromThrottledServer =>
    let s = _TestSendSSLHardCloseFromThrottledServer(_sslctx, fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendSSLHardCloseFromThrottledClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendSSLHardCloseFromThrottledClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendSSLHardCloseFromThrottledListener")

actor \nodoc\ _TestSendSSLHardCloseFromThrottledClient
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

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

actor \nodoc\ _TestSendSSLHardCloseFromThrottledServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _started: Bool = false
  var _throttled: Bool = false
  var _closed: Bool = false
  var _failed_signalled: Bool = false

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.ssl_server(
        TCPServerAuth(h.env.root),
        sslctx,
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => None
      | let _: SendError => break
      end
    end
    _h.assert_true(
      _closed,
      "_on_closed must fire before the flood loop exits")
    _h.complete_action("send accepted")
    KeepReading

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    None

  fun ref _on_throttled() =>
    _throttled = true
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _closed = true
    _h.complete_action("server closed")

  fun ref _on_sent(token: SendToken) =>
    _h.fail("a hard close drops the queued remainder, so this send must not " +
      "report as sent")

  fun ref _on_send_failed(token: SendToken) =>
    if not _failed_signalled then
      _failed_signalled = true
      _h.complete_action("token failed")
    end

class \nodoc\ iso _TestSendDeliveredNotFailedOnHardClose is UnitTest
  """
  A hard close from `_on_throttled` must not report a delivered send as failed.

  `_on_throttled` runs inside the write flush, after a partial `sendv` has
  accounted its bytes and before the flush reports the sends those bytes
  completed. A `hard_close()` from the callback fails every send still on the
  queue, so a send the flush had just finished gets `_on_send_failed` even
  though its bytes are gone. An application that believed that would resend
  them, and the peer would see them twice.

  The peer is the oracle. `hard_close()` is a plain `close(2)` and sets no
  `SO_LINGER`, so the kernel sends what is in the socket's send buffer before it
  FINs: the peer receives every byte the OS took. Sends complete in order, so a
  send whose last byte sits at cumulative offset N reached the OS once the peer
  holds N bytes, and every such send must have reported `_on_sent`. The client
  counts to its own `_on_closed`; report any earlier and it is still reading
  while bytes arrive, and it undercounts.

  The server floods 64 KiB chunks until `_on_throttled` fires, so backpressure
  triggers regardless of how much the kernel rounds `SO_SNDBUF`/`SO_RCVBUF` up.
  Twenty-five 32 KiB sends pile up behind the flood. Against a 16 KiB
  `SO_SNDBUF` a `sendv` moves a few tens of kilobytes a pass, so it lands
  mid-send far more often than on a boundary.

  POSIX only, for the same reason as `SendPerTokenCompletion`.
  """
  fun name(): String => "net/SendDeliveredNotFailedOnHardClose"

  fun ref apply(h: TestHelper) =>
    h.expect_action("split verified")

    let listener = _TestSendDeliveredListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendDeliveredListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendDeliveredServer] =
    Array[_TestSendDeliveredServer]
  var _client: (_TestSendDeliveredClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9787",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendDeliveredServer =>
    let s = _TestSendDeliveredServer(fd, _h, this)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendDeliveredClient(_h, this)

  fun ref _on_closed() =>
    try (_client as _TestSendDeliveredClient).dispose() end
    for server in _servers.values() do server.dispose() end

  be unmute_client() =>
    try (_client as _TestSendDeliveredClient).resume_reading() end

  be client_total(bytes: USize) =>
    for server in _servers.values() do server.peer_received(bytes) end

actor \nodoc\ _TestSendDeliveredClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendDeliveredListener
  var _total_received: USize = 0

  new create(h: TestHelper, listener: _TestSendDeliveredListener) =>
    _h = h
    _listener = listener
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9787",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    _tcp_connection.send("ready")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  be resume_reading() =>
    _tcp_connection.unmute()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _total_received = _total_received + data.size()
    KeepReading

  fun ref _on_closed() =>
    // Report only now. The server's close still flushes its send buffer, so
    // bytes keep arriving after it closes.
    _listener.client_total(_total_received)

actor \nodoc\ _TestSendDeliveredServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _listener: _TestSendDeliveredListener
  let _chunk: USize = 32_000
  let _total_chunks: USize = 25
  var _started: Bool = false
  var _throttled: Bool = false
  var _unmuted_client: Bool = false
  var _queued: USize = 0
  var _flood_count: USize = 0
  var _reported: Bool = false
  embed _sent_ids: Array[USize] = _sent_ids.create()
  embed _failed_ids: Array[USize] = _failed_ids.create()

  new create(fd: U32, h: TestHelper, listener: _TestSendDeliveredListener) =>
    _h = h
    _listener = listener
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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _send_bytes(n: USize) =>
    let payload = recover val Array[U8].init('x', n) end
    match \exhaustive\ _tcp_connection.send(payload)
    | SendAccepted =>
      _queued = _queued + 1
    | let _: SendError =>
      _h.fail("send() must be accepted while the connection is writeable")
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    while not _throttled do
      _send_bytes(65536)
      _flood_count = _flood_count + 1
    end
    KeepReading

  fun ref _on_unthrottled() =>
    if (_queued - _flood_count) < _total_chunks then
      _send_bytes(_chunk)
      _maybe_hard_close()
    end

  fun ref _on_throttled() =>
    _throttled = true
    if not _unmuted_client then
      _unmuted_client = true
      _listener.unmute_client()
    end
    _maybe_hard_close()

  fun ref _maybe_hard_close() =>
    if (_queued - _flood_count) == _total_chunks then
      _tcp_connection.hard_close()
    end

  fun ref _on_sent(token: SendToken) =>
    _sent_ids.push(token.id)

  fun ref _on_send_failed(token: SendToken) =>
    _failed_ids.push(token.id)

  be peer_received(bytes: USize) =>
    if _reported then return end
    _reported = true
    var reached_os: USize = 0
    var offset: USize = 0
    var k: USize = 0
    while k < _queued do
      offset = offset + if k < _flood_count then USize(65536) else _chunk end
      if offset <= bytes then
        reached_os = reached_os + 1
      end
      k = k + 1
    end
    _h.log("peer=" + bytes.string() + " reached_os=" + reached_os.string() +
      " on_sent=" + _sent_ids.size().string() +
      " on_send_failed=" + _failed_ids.size().string())
    _h.assert_true(
      _sent_ids.size() >= reached_os,
      "the peer holds " + bytes.string() + " bytes, so " + reached_os.string() +
        " sends reached the OS, but only " + _sent_ids.size().string() +
        " reported _on_sent")
    _h.complete_action("split verified")

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestSendAcceptedBeforeSent is UnitTest
  """
  A send's token reaches `_on_send_accepted` before it reaches `_on_sent`, and
  `_on_send_accepted` gets the same data `send()` was given.

  The client makes one `ByteSeq` send and one `ByteSeqIter` send, so both arms
  of `send()`'s parameter reach the callback. An application that keys its
  bookkeeping off the token has nothing to record a completion against if
  `_on_sent` can arrive for a token it has not been handed yet.
  """
  fun name(): String => "net/SendAcceptedBeforeSent"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("byteseq data verified")
    h.expect_action("byteseqiter data verified")
    h.expect_action("both sends completed")

    let s = _TestSendAcceptedBeforeSentListener(h)
    h.dispose_when_done(s)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendAcceptedBeforeSentListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]
  var _client: (_TestSendAcceptedBeforeSentClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7922",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let s = _TestDoNothingServerActor(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendAcceptedBeforeSentClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _h.complete_action("server listening")
    _client = _TestSendAcceptedBeforeSentClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendAcceptedBeforeSentListener")

actor \nodoc\ _TestSendAcceptedBeforeSentClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  embed _accepted: Array[USize] = _accepted.create()
  embed _sent: Array[USize] = _sent.create()

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7922",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    match \exhaustive\ _tcp_connection.send("hello")
    | SendAccepted => None
    | let _: SendError => _h.fail("the ByteSeq send was refused")
    end
    match \exhaustive\ _tcp_connection.send(
      recover val [as ByteSeq: "a"; "bc"] end)
    | SendAccepted => None
    | let _: SendError => _h.fail("the ByteSeqIter send was refused")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _h.assert_false(
      _sent.contains(token.id),
      "_on_sent fired for a token before _on_send_accepted delivered it")
    _accepted.push(token.id)

    match data
    | let s: String =>
      _h.assert_eq[String](
        "hello",
        s,
        "_on_send_accepted got a different ByteSeq than send() was given")
      _h.complete_action("byteseq data verified")
    | let i: ByteSeqIter =>
      let joined =
        recover val
          let b = String
          for chunk in i.values() do
            b.append(chunk)
          end
          b
        end
      _h.assert_eq[String](
        "abc",
        joined,
        "_on_send_accepted got a different ByteSeqIter than send() was given")
      _h.complete_action("byteseqiter data verified")
    else
      _h.fail("_on_send_accepted got data of an unexpected type")
    end

  fun ref _on_sent(token: SendToken) =>
    _h.assert_true(
      _accepted.contains(token.id),
      "_on_sent fired for a token _on_send_accepted never delivered")
    _sent.push(token.id)
    if _sent.size() == 2 then
      _h.complete_action("both sends completed")
      _tcp_connection.close()
    end

class \nodoc\ iso _TestSendCloseFromAccepted is UnitTest
  """
  A graceful `close()` from inside `_on_send_accepted` still sends the FIN.

  `_on_send_accepted` runs after `send()` has queued the bytes and before it
  flushes them, so a `close()` there moves the connection to `_Closing` while
  the queue is non-empty, and `_initiate_shutdown()` defers the FIN. The flush
  that follows empties the queue and the FIN goes out on that drain. Without
  it the connection sits in `_Closing` with its write side never shut, and the
  peer waits for a close that never comes.

  Connects to a literal address rather than `localhost`, which resolves to more
  than one. A second Happy Eyeballs attempt leaves inflight events in the
  array, which defers the FIN on its own, and the straggler's cleanup then
  sends it -- so the test would pass whether or not the drain does.
  """
  fun name(): String => "net/SendCloseFromAccepted"

  fun apply(h: TestHelper) =>
    h.expect_action("send accepted")
    h.expect_action("token sent")
    h.expect_action("server saw payload")
    h.expect_action("server saw close")

    let s = _TestSendCloseFromAcceptedListener(h)
    h.dispose_when_done(s)

    h.long_test(10_000_000_000)

actor \nodoc\ _TestSendCloseFromAcceptedListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendCloseFromAcceptedServer] =
    Array[_TestSendCloseFromAcceptedServer]
  var _client: (_TestSendCloseFromAcceptedClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "127.0.0.1" end,
        "7923",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendCloseFromAcceptedServer =>
    let s = _TestSendCloseFromAcceptedServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendCloseFromAcceptedClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendCloseFromAcceptedClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendCloseFromAcceptedListener")

actor \nodoc\ _TestSendCloseFromAcceptedClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "127.0.0.1" end,
        "7923",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    match \exhaustive\ _tcp_connection.send("payload")
    | SendAccepted => _h.complete_action("send accepted")
    | let _: SendError => _h.fail("send was refused on an open connection")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _tcp_connection.close()

  fun ref _on_sent(token: SendToken) =>
    _h.complete_action("token sent")

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("a graceful close flushes queued writes, it does not fail them")

actor \nodoc\ _TestSendCloseFromAcceptedServer
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
    match MakeBufferSize(7)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.assert_eq[String]("payload", String.from_array(consume data))
    _h.complete_action("server saw payload")
    KeepReading

  fun ref _on_closed() =>
    _h.complete_action("server saw close")

class \nodoc\ iso _TestSendCloseFromSent is UnitTest
  """
  A graceful `close()` from inside `_on_sent` still sends the FIN.

  `_on_sent` fires from the flush that emptied the write queue, so a `close()`
  there finds nothing pending and `_initiate_shutdown()` sends the FIN
  immediately. `SendCloseFromAccepted` covers the other half, where the queue
  is still full when `close()` runs; this one guards the path that does not go
  through `_Closing.drained`, so a change to when the FIN goes out cannot break
  it unnoticed.

  Connects to a literal address for the same reason as `SendCloseFromAccepted`.
  """
  fun name(): String => "net/SendCloseFromSent"

  fun apply(h: TestHelper) =>
    h.expect_action("token sent")
    h.expect_action("server saw payload")
    h.expect_action("server saw close")

    let s = _TestSendCloseFromSentListener(h)
    h.dispose_when_done(s)

    h.long_test(10_000_000_000)

actor \nodoc\ _TestSendCloseFromSentListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendCloseFromSentServer] =
    Array[_TestSendCloseFromSentServer]
  var _client: (_TestSendCloseFromSentClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "127.0.0.1" end,
        "7924",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendCloseFromSentServer =>
    let s = _TestSendCloseFromSentServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendCloseFromSentClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendCloseFromSentClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendCloseFromSentListener")

actor \nodoc\ _TestSendCloseFromSentClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "127.0.0.1" end,
        "7924",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    match \exhaustive\ _tcp_connection.send("payload")
    | SendAccepted => None
    | let _: SendError => _h.fail("send was refused on an open connection")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_sent(token: SendToken) =>
    _h.complete_action("token sent")
    _tcp_connection.close()

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("the payload reached the OS, so it must not report as failed")

actor \nodoc\ _TestSendCloseFromSentServer
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
    match MakeBufferSize(7)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _h.assert_eq[String]("payload", String.from_array(consume data))
    _h.complete_action("server saw payload")
    KeepReading

  fun ref _on_closed() =>
    _h.complete_action("server saw close")

class \nodoc\ iso _TestSendHardCloseFromAccepted is UnitTest
  """
  A `hard_close()` from inside `_on_send_accepted` still reports the send as
  accepted, and its token fires `_on_send_failed`.

  The token is already on the pending queue when `_on_send_accepted` runs, so
  the hard close finds it there and fails it, and `send()` returns
  `SendAccepted` to a caller whose connection is already closed. Reporting the
  send as refused instead would leave the application with a token from
  `_on_send_accepted` and a `SendError` from the same call.
  """
  fun name(): String => "net/SendHardCloseFromAccepted"

  fun apply(h: TestHelper) =>
    h.expect_action("send accepted")
    h.expect_action("client closed")
    h.expect_action("token failed")

    let s = _TestSendHardCloseFromAcceptedListener(h)
    h.dispose_when_done(s)

    h.long_test(10_000_000_000)

actor \nodoc\ _TestSendHardCloseFromAcceptedListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]
  var _client: (_TestSendHardCloseFromAcceptedClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7925",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let s = _TestDoNothingServerActor(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendHardCloseFromAcceptedClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendHardCloseFromAcceptedClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendHardCloseFromAcceptedListener")

actor \nodoc\ _TestSendHardCloseFromAcceptedClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _token: (SendToken | None) = None
  var _closed: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7925",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    match \exhaustive\ _tcp_connection.send("payload")
    | SendAccepted =>
      _h.assert_true(
        _closed,
        "the hard close from _on_send_accepted must have already run")
      _h.complete_action("send accepted")
    | let _: SendError =>
      _h.fail("send() must report accepted: _on_send_accepted already ran")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _h.assert_false(_closed, "_on_send_accepted must run before the close")
    _token = token
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _closed = true
    _h.complete_action("client closed")

  fun ref _on_sent(token: SendToken) =>
    _h.fail("a hard close drops the queued bytes, so this must not report sent")

  fun ref _on_send_failed(token: SendToken) =>
    match _token
    | let t: SendToken if token == t =>
      _h.complete_action("token failed")
    else
      _h.fail("_on_send_failed fired with an unexpected token")
    end

class \nodoc\ iso _TestSendReentrantFromSent is UnitTest
  """
  A `send()` made from inside `_on_sent` still completes in send order.

  `_on_sent` fires from the flush inside `send()`, so sending the next message
  from it nests one `send()` inside another. The client chains five that way.
  Each send is minted, flushed and reported from inside the one before it, and
  the ids still have to come out 1 through 5.

  Each link completes before the next is issued, so this never has two tokens
  queued at once. `SendHardCloseFromSentKeepsDelivered` and
  `SendThrottleSuppressedByHardClose` are the ones that do.
  """
  fun name(): String => "net/SendReentrantFromSent"

  fun apply(h: TestHelper) =>
    h.expect_action("all sends completed in order")

    let s = _TestSendReentrantFromSentListener(h)
    h.dispose_when_done(s)

    h.long_test(10_000_000_000)

actor \nodoc\ _TestSendReentrantFromSentListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]
  var _client: (_TestSendReentrantFromSentClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7926",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let s = _TestDoNothingServerActor(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendReentrantFromSentClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendReentrantFromSentClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendReentrantFromSentListener")

actor \nodoc\ _TestSendReentrantFromSentClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  let _total: USize = 5
  var _issued: USize = 0
  var _next_expected: USize = 1

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7926",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _send_next()

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _send_next() =>
    _issued = _issued + 1
    match \exhaustive\ _tcp_connection.send("m" + _issued.string())
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send " + _issued.string() + " was refused")
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _h.assert_eq[USize](
      _issued,
      token.id,
      "_on_send_accepted delivered a token out of send order")

  fun ref _on_sent(token: SendToken) =>
    _h.assert_eq[USize](_next_expected, token.id, "_on_sent fired out of order")
    _next_expected = _next_expected + 1
    if token.id == _total then
      _h.complete_action("all sends completed in order")
      _tcp_connection.close()
    else
      _send_next()
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("no send should fail on an idle loopback connection")

class \nodoc\ iso _TestSendOnSentPrecedesThrottleAndClose is UnitTest
  """
  `_on_sent` for a send that already reached the OS arrives before
  `_on_throttled`, and before the `_on_closed` of a hard close made from
  `_on_throttled`.

  The server sends a two-byte "go" that drains in full, then floods 64 KiB
  chunks until `_on_throttled` fires. The small send's bytes were with the OS
  first, so its `_on_sent` has to arrive before `_on_throttled`. Delivering
  it in a later behavior turn puts it after both, telling an application that
  a send completed after it was told the connection had closed.

  POSIX only, for the same reason as `SendPerTokenCompletion`.
  """
  fun name(): String => "net/SendOnSentPrecedesThrottleAndClose"

  fun ref apply(h: TestHelper) =>
    h.expect_action("sent before throttled")
    h.expect_action("throttled send refused")
    h.expect_action("sent before closed")
    h.expect_action("large send failed")

    let listener = _TestSendOnSentPrecedesListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendOnSentPrecedesListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendOnSentPrecedesServer] =
    Array[_TestSendOnSentPrecedesServer]
  var _client: (_TestSendOnSentPrecedesClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7927",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendOnSentPrecedesServer =>
    let s = _TestSendOnSentPrecedesServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendOnSentPrecedesClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendOnSentPrecedesClient).dispose() end
    for server in _servers.values() do server.dispose() end

actor \nodoc\ _TestSendOnSentPrecedesClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7927",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    // Muted with a small receive buffer so the pipe fills and stays full.
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    match \exhaustive\ _tcp_connection.send("ready")
    | SendAccepted => None
    | let _: SendError => _h.fail("client could not send its ready message")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

actor \nodoc\ _TestSendOnSentPrecedesServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _started: Bool = false
  var _throttled: Bool = false
  var _small_token: (SendToken | None) = None
  var _small_sent: Bool = false
  var _large_failed: Bool = false
  var _accepted: USize = 0

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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    match \exhaustive\ _tcp_connection.send("go")
    | SendAccepted => None
    | let _: SendError => _h.fail("the small send was refused")
    end
    while not _throttled do
      let chunk = recover val Array[U8].init('x', 65536) end
      match \exhaustive\ _tcp_connection.send(chunk)
      | SendAccepted => None
      | let _: SendError => break
      end
    end
    KeepReading

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted = _accepted + 1
    if _small_token is None then
      _small_token = token
    end

  fun ref _on_sent(token: SendToken) =>
    match _small_token
    | let t: SendToken if token == t => _small_sent = true
    end

  fun ref _on_throttled() =>
    _throttled = true
    _h.assert_true(
      _small_sent,
      "_on_sent for the drained send must arrive before _on_throttled")
    _h.complete_action("sent before throttled")

    let accepted_before = _accepted
    match \exhaustive\ _tcp_connection.send("nope")
    | SendAccepted =>
      _h.fail("send() must be refused while the connection is throttled")
    | SendErrorNotWriteable =>
      _h.assert_eq[USize](
        accepted_before,
        _accepted,
        "a refused send must not fire _on_send_accepted")
      _h.complete_action("throttled send refused")
    | SendErrorNotConnected =>
      _h.fail("the connection is throttled, not closed")
    end

    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _h.assert_true(
      _small_sent,
      "_on_sent for the drained send must arrive before _on_closed")
    _h.complete_action("sent before closed")

  fun ref _on_send_failed(token: SendToken) =>
    if not _large_failed then
      _large_failed = true
      _h.complete_action("large send failed")
    end

class \nodoc\ iso _TestSendThrottleSuppressedByHardClose is UnitTest
  """
  `_on_throttled` does not fire when the application hard-closes from the
  `_on_sent` that backpressure itself delivered.

  Sending from inside `_on_send_accepted` queues a second send behind the first
  before either has been written, so one `sendv` covers both: the two-byte
  first send completes and the 4 MiB second one partial-writes. That raises
  backpressure, which reports the completed send before it reports the
  backpressure -- and the application hard-closes from that `_on_sent`. The
  connection is no longer throttled and has already had `_on_closed` by the
  time `_on_throttled` would fire, so it must not fire at all.

  POSIX only, for the same reason as `SendPerTokenCompletion`.
  """
  fun name(): String => "net/SendThrottleSuppressedByHardClose"

  fun ref apply(h: TestHelper) =>
    h.expect_action("closed without throttling")
    h.expect_action("large send failed")

    let listener = _TestSendThrottleSuppressedListener(h)
    h.dispose_when_done(listener)
    h.long_test(30_000_000_000)

actor \nodoc\ _TestSendThrottleSuppressedListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendThrottleSuppressedServer] =
    Array[_TestSendThrottleSuppressedServer]
  var _client: (_TestSendThrottleSuppressedClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7928",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendThrottleSuppressedServer =>
    let s = _TestSendThrottleSuppressedServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_listen_failure() =>
    _h.fail("listener failed to start")

  fun ref _on_listening() =>
    _client = _TestSendThrottleSuppressedClient(_h)

  fun ref _on_closed() =>
    try (_client as _TestSendThrottleSuppressedClient).dispose() end
    for server in _servers.values() do server.dispose() end

actor \nodoc\ _TestSendThrottleSuppressedClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "7928",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    // Muted with a small receive buffer so the pipe fills and stays full.
    ifdef bsd then
      _tcp_connection.set_so_rcvbuf(16384)
    else
      _tcp_connection.set_so_rcvbuf(4096)
    end
    _tcp_connection.mute()
    match \exhaustive\ _tcp_connection.send("ready")
    | SendAccepted => None
    | let _: SendError => _h.fail("client could not send its ready message")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

actor \nodoc\ _TestSendThrottleSuppressedServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _started: Bool = false
  var _small_token: (SendToken | None) = None
  var _large_token: (SendToken | None) = None
  var _small_sent: Bool = false

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
    match MakeBufferSize(5)
    | let b: BufferSize => _tcp_connection.buffer_until(b)
    else _Unreachable()
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _started then return KeepReading end
    _started = true
    _tcp_connection.set_so_sndbuf(16384)
    match \exhaustive\ _tcp_connection.send("go")
    | SendAccepted => None
    | let _: SendError => _h.fail("the small send was refused")
    end
    KeepReading

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    match _small_token
    | None =>
      _small_token = token
      // Queue the large send behind the small one before either has been
      // written, so one sendv completes the small one and partial-writes the
      // large one.
      match \exhaustive\ _tcp_connection.send(
        recover val Array[U8].init('x', 4_194_304) end)
      | SendAccepted => None
      | let _: SendError => _h.fail("the large send was refused")
      end
    else
      _large_token = token
    end

  fun ref _on_sent(token: SendToken) =>
    match _small_token
    | let t: SendToken if token == t =>
      _small_sent = true
      _tcp_connection.hard_close()
    else
      _h.fail("only the small send reaches the OS before the hard close")
    end

  fun ref _on_throttled() =>
    _h.fail("_on_throttled must not fire once the connection has closed")

  fun ref _on_closed() =>
    _h.assert_true(_small_sent, "_on_sent must arrive before _on_closed")
    _h.complete_action("closed without throttling")

  fun ref _on_send_failed(token: SendToken) =>
    match _large_token
    | let t: SendToken if token == t =>
      _h.complete_action("large send failed")
    else
      _h.fail("_on_send_failed fired for a send that reached the OS")
    end

class \nodoc\ iso _TestSendHardCloseFromSentKeepsDelivered is UnitTest
  """
  A `hard_close()` from inside `_on_sent` does not turn an already-delivered
  send into a failure.

  Sending from inside `_on_send_accepted` puts two tokens on the queue before
  either is written, so one `sendv` completes both. `_fire_completed_sends`
  reports them one at a time, and the application hard-closes from the first
  one's `_on_sent` while the second is still queued. Both sends' bytes are with
  the OS and the peer receives them, so both have to report `_on_sent`. Failing
  the second would tell an application to resend bytes the peer already has.

  Both sends are four bytes on an idle loopback socket, so the write that
  covers them is not partial.
  """
  fun name(): String => "net/SendHardCloseFromSentKeepsDelivered"

  fun apply(h: TestHelper) =>
    h.expect_action("both sends reported sent")
    h.expect_action("client closed")
    h.expect_action("server saw both payloads")

    let s = _TestSendKeepsDeliveredListener(h)
    h.dispose_when_done(s)

    h.long_test(10_000_000_000)

actor \nodoc\ _TestSendKeepsDeliveredListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendKeepsDeliveredServer] =
    Array[_TestSendKeepsDeliveredServer]
  var _client: (_TestSendKeepsDeliveredClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7929",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendKeepsDeliveredServer =>
    let s = _TestSendKeepsDeliveredServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendKeepsDeliveredClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendKeepsDeliveredClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendKeepsDeliveredListener")

actor \nodoc\ _TestSendKeepsDeliveredClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _nested_sent: Bool = false
  var _closed: Bool = false
  embed _sent: Array[USize] = _sent.create()

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7929",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    match \exhaustive\ _tcp_connection.send("AAAA")
    | SendAccepted => None
    | let _: SendError => _h.fail("the first send was refused")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    if _nested_sent then return end
    _nested_sent = true
    // Queued behind the first send, before either has been written.
    match \exhaustive\ _tcp_connection.send("BBBB")
    | SendAccepted => None
    | let _: SendError => _h.fail("the nested send was refused")
    end

  fun ref _on_sent(token: SendToken) =>
    _h.assert_false(_closed, "_on_sent must arrive before _on_closed")
    _sent.push(token.id)
    if _sent.size() == 1 then
      _tcp_connection.hard_close()
    elseif _sent.size() == 2 then
      _h.complete_action("both sends reported sent")
    end

  fun ref _on_send_failed(token: SendToken) =>
    _h.fail("both sends reached the OS, so neither may report as failed")

  fun ref _on_closed() =>
    _closed = true
    _h.assert_eq[USize](
      2,
      _sent.size(),
      "both sends must report _on_sent before _on_closed")
    _h.complete_action("client closed")

actor \nodoc\ _TestSendKeepsDeliveredServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _received: String ref = String

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
    _received.append(String.from_array(consume data))
    if _received == "AAAABBBB" then
      _h.complete_action("server saw both payloads")
    end
    KeepReading

class \nodoc\ iso _TestSendOnSentPrecedesReceived is UnitTest
  """
  `_on_sent` for a completed write arrives before the `_on_received` for data
  read after it.

  The read loop takes messages out one at a time, so a send made from
  `_on_received` for one message completes before the next message is
  delivered. The server frames at four bytes and the client sends eight in one
  call, so both messages come out of the same read: the server answers the
  first, and its answer's `_on_sent` has to arrive before the second message.
  Delivering `_on_sent` in a later behavior turn puts it after both.
  """
  fun name(): String => "net/SendOnSentPrecedesReceived"

  fun apply(h: TestHelper) =>
    h.expect_action("second message saw the first send completed")

    let s = _TestSendPrecedesReceivedListener(h)
    h.dispose_when_done(s)

    h.long_test(10_000_000_000)

actor \nodoc\ _TestSendPrecedesReceivedListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  let _servers: Array[_TestSendPrecedesReceivedServer] =
    Array[_TestSendPrecedesReceivedServer]
  var _client: (_TestSendPrecedesReceivedClient | None) = None

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "7930",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSendPrecedesReceivedServer =>
    let s = _TestSendPrecedesReceivedServer(fd, _h)
    _servers.push(s)
    s

  fun ref _on_closed() =>
    try (_client as _TestSendPrecedesReceivedClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestSendPrecedesReceivedClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSendPrecedesReceivedListener")

actor \nodoc\ _TestSendPrecedesReceivedClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "localhost",
        "7930",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    // Two four-byte frames in one write, so the server reads both at once.
    match \exhaustive\ _tcp_connection.send("AAAABBBB")
    | SendAccepted => None
    | let _: SendError => _h.fail("the client send was refused")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client connect failed")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    KeepReading

actor \nodoc\ _TestSendPrecedesReceivedServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _received: USize = 0
  var _sent: USize = 0

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
    else _Unreachable()
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received = _received + 1
    if _received == 1 then
      match \exhaustive\ _tcp_connection.send("ok")
      | SendAccepted => None
      | let _: SendError => _h.fail("the server answer was refused")
      end
    elseif _received == 2 then
      _h.assert_eq[USize](
        1,
        _sent,
        "_on_sent for the answer must arrive before the next message")
      _h.complete_action("second message saw the first send completed")
      _tcp_connection.close()
    end
    KeepReading

  fun ref _on_sent(token: SendToken) =>
    _sent = _sent + 1

// ===========================================================================
// Fake-backend send tests (no real sockets for I/O)
// ===========================================================================
class \nodoc\ iso _TestFakeSendOk is UnitTest
  """
  sendv accepts all bytes. Verify: send returns SendAccepted,
  _on_send_accepted fires with a token and the data, _on_sent fires with
  the same token.
  """
  fun name(): String => "net/FakeSendOk"

  fun apply(h: TestHelper) =>
    h.expect_action("on_send_accepted")
    h.expect_action("on_sent")

    let a = _TestFakeSendOkActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendOkActor
  is (TCPConnectionActor[_FBSendOkRecvRetry]
    & ServerLifecycleEventReceiver[_FBSendOkRecvRetry])
  var _tcp_connection: TCPConnection[_FBSendOkRecvRetry] =
    TCPConnection[_FBSendOkRecvRetry].none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

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
    match \exhaustive\ _tcp_connection.send("hello")
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send returned an error")
      _h.complete(false)
    end

  fun ref _on_send_accepted(
    token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _expected_token = token
    _h.complete_action("on_send_accepted")

  fun ref _on_sent(token: SendToken) =>
    match \exhaustive\ _expected_token
    | let expected: SendToken =>
      _h.assert_true(token == expected, "token mismatch")
      _h.complete_action("on_sent")
    | None =>
      _h.fail("_on_sent fired without _on_send_accepted")
    end

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeSendMultipleTokenOrder is UnitTest
  """
  Three sends all succeed. Verify: _on_send_accepted fires three times with
  distinct tokens, and _on_sent fires three times in the same order.
  """
  fun name(): String => "net/FakeSendMultipleTokenOrder"

  fun apply(h: TestHelper) =>
    h.expect_action("all_sent")

    let a = _TestFakeSendMultipleTokenOrderActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendMultipleTokenOrderActor
  is (TCPConnectionActor[_FBSendOkRecvRetry]
    & ServerLifecycleEventReceiver[_FBSendOkRecvRetry])
  var _tcp_connection: TCPConnection[_FBSendOkRecvRetry] =
    TCPConnection[_FBSendOkRecvRetry].none()
  let _h: TestHelper
  let _expected_tokens: Array[SendToken] = Array[SendToken]
  var _sent_count: USize = 0

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
    for payload in ["aaa"; "bbb"; "ccc"].values() do
      match \exhaustive\ _tcp_connection.send(payload)
      | SendAccepted => None
      | let _: SendError =>
        _h.fail("send returned an error")
        _h.complete(false)
        return
      end
    end

  fun ref _on_send_accepted(
    token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _expected_tokens.push(token)

  fun ref _on_sent(token: SendToken) =>
    try
      let expected = _expected_tokens(_sent_count)?
      _h.assert_true(
        token == expected,
        "token order mismatch at index " + _sent_count.string())
    else
      _h.fail("_on_sent with no matching _on_send_accepted")
    end
    _sent_count = _sent_count + 1
    if _sent_count == 3 then
      _h.complete_action("all_sent")
    end

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeSendOnClosed is UnitTest
  """
  send() on a hard-closed connection returns SendErrorNotConnected without
  firing _on_send_accepted.
  """
  fun name(): String => "net/FakeSendOnClosed"

  fun apply(h: TestHelper) =>
    h.expect_action("send_error_verified")

    let a = _TestFakeSendOnClosedActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendOnClosedActor
  is (TCPConnectionActor[_FBSendOkRecvRetry]
    & ServerLifecycleEventReceiver[_FBSendOkRecvRetry])
  var _tcp_connection: TCPConnection[_FBSendOkRecvRetry] =
    TCPConnection[_FBSendOkRecvRetry].none()
  let _h: TestHelper
  var _send_accepted_after_close: Bool = false

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
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    match \exhaustive\ _tcp_connection.send("hello")
    | SendAccepted =>
      _h.fail("send on closed connection returned SendAccepted")
      _h.complete(false)
    | SendErrorNotConnected =>
      _h.complete_action("send_error_verified")
    | SendErrorNotWriteable =>
      _h.fail("send on closed connection returned SendErrorNotWriteable")
      _h.complete(false)
    end

  fun ref _on_send_accepted(
    token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _send_accepted_after_close = true
    _h.fail("_on_send_accepted fired after hard_close")
    _h.complete(false)

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeSendError is UnitTest
  """
  sendv returns error. Verify: _on_send_accepted fires (token minted before
  flush), then _on_closed fires (hard close from sendv error), then
  _on_send_failed fires with the token (deferred).
  """
  fun name(): String => "net/FakeSendError"

  fun apply(h: TestHelper) =>
    h.expect_action("on_send_accepted")
    h.expect_action("on_closed")
    h.expect_action("on_send_failed")

    let a = _TestFakeSendErrorActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendErrorActor
  is (TCPConnectionActor[_FBSendErrorRecvRetry]
    & ServerLifecycleEventReceiver[_FBSendErrorRecvRetry])
  var _tcp_connection: TCPConnection[_FBSendErrorRecvRetry] =
    TCPConnection[_FBSendErrorRecvRetry].none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendErrorRecvRetry].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendErrorRecvRetry] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.mute()
    match \exhaustive\ _tcp_connection.send("hello")
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send returned an error before flush")
      _h.complete(false)
    end

  fun ref _on_send_accepted(
    token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _expected_token = token
    _h.complete_action("on_send_accepted")

  fun ref _on_closed() =>
    _h.complete_action("on_closed")

  fun ref _on_send_failed(token: SendToken) =>
    match \exhaustive\ _expected_token
    | let expected: SendToken =>
      _h.assert_true(token == expected, "send_failed token mismatch")
      _h.complete_action("on_send_failed")
    | None =>
      _h.fail("_on_send_failed without _on_send_accepted")
    end

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeSendFailedAfterClosed is UnitTest
  """
  _on_send_failed must arrive after _on_closed when hard_close is called with
  queued sends. Verify: send queues bytes (sendv returns retry), hard_close
  fires _on_closed synchronously, then _on_send_failed arrives in a
  subsequent behavior turn.
  """
  fun name(): String => "net/FakeSendFailedAfterClosed"

  fun apply(h: TestHelper) =>
    h.expect_action("on_closed")
    h.expect_action("on_send_failed_after_closed")

    let a = _TestFakeSendFailedAfterClosedActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeSendFailedAfterClosedActor
  is (TCPConnectionActor[_FBSendStepRecvRetryFailed]
    & ServerLifecycleEventReceiver[_FBSendStepRecvRetryFailed])
  var _tcp_connection: TCPConnection[_FBSendStepRecvRetryFailed] =
    TCPConnection[_FBSendStepRecvRetryFailed].none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None
  var _closed: Bool = false

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendStepRecvRetryFailed].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection(): TCPConnection[_FBSendStepRecvRetryFailed] =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.mute()
    match \exhaustive\ _tcp_connection.send("hello")
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send returned an error")
      _h.complete(false)
      return
    end
    _tcp_connection.hard_close()

  fun ref _on_send_accepted(
    token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _expected_token = token

  fun ref _on_closed() =>
    _closed = true
    _h.complete_action("on_closed")

  fun ref _on_send_failed(token: SendToken) =>
    if not _closed then
      _h.fail("_on_send_failed fired before _on_closed")
      _h.complete(false)
      return
    end
    match \exhaustive\ _expected_token
    | let expected: SendToken =>
      _h.assert_true(token == expected, "send_failed token mismatch")
      _h.complete_action("on_send_failed_after_closed")
    | None =>
      _h.fail("_on_send_failed without _on_send_accepted")
    end

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestFakeGracefulClose is UnitTest
  """
  Graceful close after a completed send. Verify: _on_sent fires, and
  close() makes subsequent send() return SendErrorNotConnected (_Closing
  rejects sends the same way _Closed does).
  """
  fun name(): String => "net/FakeGracefulClose"

  fun apply(h: TestHelper) =>
    h.expect_action("on_sent")
    h.expect_action("close_rejects_send")

    let a = _TestFakeGracefulCloseActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestFakeGracefulCloseActor
  is (TCPConnectionActor[_FBSendOkRecvRetry]
    & ServerLifecycleEventReceiver[_FBSendOkRecvRetry])
  var _tcp_connection: TCPConnection[_FBSendOkRecvRetry] =
    TCPConnection[_FBSendOkRecvRetry].none()
  let _h: TestHelper
  var _expected_token: (SendToken | None) = None

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
    match \exhaustive\ _tcp_connection.send("aaa")
    | SendAccepted => None
    | let _: SendError =>
      _h.fail("send returned an error")
      _h.complete(false)
      return
    end
    _tcp_connection.close()
    match \exhaustive\ _tcp_connection.send("bbb")
    | SendAccepted =>
      _h.fail("send after close should fail")
      _h.complete(false)
    | SendErrorNotConnected =>
      _h.complete_action("close_rejects_send")
    | SendErrorNotWriteable =>
      _h.fail("send after close returned SendErrorNotWriteable")
      _h.complete(false)
    end

  fun ref _on_send_accepted(
    token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _expected_token = token

  fun ref _on_sent(token: SendToken) =>
    match \exhaustive\ _expected_token
    | let expected: SendToken =>
      _h.assert_true(token == expected, "token mismatch")
      _h.complete_action("on_sent")
    | None =>
      _h.fail("_on_sent without _on_send_accepted")
    end

  be dispose() =>
    _tcp_connection.hard_close()

