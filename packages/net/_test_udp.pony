use "pony_test"

primitive \nodoc\ _FakeUDPFd
  """
  Allocate a raw UDP socket fd suitable for PonyAsio.create_event. The fd is
  never bound — it exists only so ASIO has a valid file descriptor. The fake
  backend handles all I/O.
  """
  fun apply(): U32 ? =>
    let fd = @socket(I32(2), I32(2), I32(0))
    if fd < 0 then error end
    fd.u32()

// Fake UDP backends
class \nodoc\ _FBUDPBindFail is UDPBackend
  """
  Fake backend: bind always fails (returns null event).
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref close(fd: U32) => None

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultError, 0, recover iso NetAddress end)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultError

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBUDPSendOk is UDPBackend
  """
  Fake backend: bind succeeds, sendto returns Ok, recvfrom returns retry.
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    try
      @pony_asio_event_create(
        the_actor,
        _FakeUDPFd()?,
        AsioEvent.read_write_oneshot(),
        0,
        true)
    else
      AsioEvent.none()
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultRetry, 0, recover iso NetAddress end)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBUDPSendWouldBlock is UDPBackend
  """
  Fake backend: bind succeeds, sendto returns Retry (EWOULDBLOCK).
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    try
      @pony_asio_event_create(
        the_actor,
        _FakeUDPFd()?,
        AsioEvent.read_write_oneshot(),
        0,
        true)
    else
      AsioEvent.none()
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultRetry, 0, recover iso NetAddress end)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultRetry

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBUDPSendError is UDPBackend
  """
  Fake backend: bind succeeds, sendto returns Error.
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    try
      @pony_asio_event_create(
        the_actor,
        _FakeUDPFd()?,
        AsioEvent.read_write_oneshot(),
        0,
        true)
    else
      AsioEvent.none()
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultRetry, 0, recover iso NetAddress end)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultError

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBUDPRecvHello is UDPBackend
  """
  Fake backend: bind succeeds, first recvfrom delivers "hello",
  subsequent ones return retry.
  """
  var _step: USize = 0

  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    try
      @pony_asio_event_create(
        the_actor,
        _FakeUDPFd()?,
        AsioEvent.read_write_oneshot(),
        0,
        true)
    else
      AsioEvent.none()
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    if _step == 0 then
      _step = 1
      let msg = "hello"
      @memcpy(buffer, msg.cpointer(), msg.size())
      (SocketResultOk, msg.size(), recover iso NetAddress end)
    else
      (SocketResultRetry, 0, recover iso NetAddress end)
    end

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

// Test classes
class \nodoc\ iso _TestUDPBindFailure is UnitTest
  """
  _on_bind_failure fires when the backend's bind returns a null event.
  """
  fun name(): String => "net/UDPBindFailure"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPBindFailureActor[_FBUDPBindFail](h)

actor \nodoc\ _TestUDPBindFailureActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9800", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    _h.fail("Expected bind failure, got _on_bound")
    _h.complete(false)

  fun ref _on_bind_failure() =>
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeSendOk is UnitTest
  """
  send_to returns SendToOk when the backend's sendto returns Ok.
  """
  fun name(): String => "net/UDPFakeSendOk"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeSendOkActor[_FBUDPSendOk](h)

actor \nodoc\ _TestUDPFakeSendOkActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9801", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    let dest = recover val NetAddress end
    match \exhaustive\ _udp.send_to("test", dest)
    | SendToOk =>
      _udp.close()
    | let e: SendToFailure =>
      _h.fail("Expected SendToOk")
      _h.complete(false)
    end

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeSendWouldBlock is UnitTest
  """
  send_to returns SendToWouldBlock when the backend's sendto returns Retry.
  """
  fun name(): String => "net/UDPFakeSendWouldBlock"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeSendWouldBlockActor[_FBUDPSendWouldBlock](h)

actor \nodoc\ _TestUDPFakeSendWouldBlockActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9802", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    let dest = recover val NetAddress end
    match _udp.send_to("test", dest)
    | SendToWouldBlock =>
      _udp.close()
    else
      _h.fail("Expected SendToWouldBlock")
      _h.complete(false)
    end

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeSendError is UnitTest
  """
  send_to returns SendToError when the backend's sendto returns Error.
  """
  fun name(): String => "net/UDPFakeSendError"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeSendErrorActor[_FBUDPSendError](h)

actor \nodoc\ _TestUDPFakeSendErrorActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9803", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    let dest = recover val NetAddress end
    match _udp.send_to("test", dest)
    | SendToError =>
      _udp.close()
    else
      _h.fail("Expected SendToError")
      _h.complete(false)
    end

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeSendNotOpen is UnitTest
  """
  send_to returns SendToNotOpen when the socket is closed.
  """
  fun name(): String => "net/UDPFakeSendNotOpen"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeSendNotOpenActor[_FBUDPSendOk](h)

actor \nodoc\ _TestUDPFakeSendNotOpenActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9804", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    let dest = recover val NetAddress end
    match _udp.send_to("test", dest)
    | SendToNotOpen =>
      _h.complete(true)
    else
      _h.fail("Expected SendToNotOpen")
      _h.complete(false)
    end

class \nodoc\ iso _TestUDPFakeRecvData is UnitTest
  """
  _on_received delivers data from recvfrom.
  """
  fun name(): String => "net/UDPFakeRecvData"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeRecvDataActor[_FBUDPRecvHello](h)

actor \nodoc\ _TestUDPFakeRecvDataActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9805", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    _h.assert_eq[String]("hello", s)
    _udp.close()
    KeepReading

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeCloseFromReceived is UnitTest
  """
  Closing from _on_received stops the read loop immediately.
  """
  fun name(): String => "net/UDPFakeCloseFromReceived"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeCloseFromReceivedActor[_FBUDPRecvAlways](h)

actor \nodoc\ _TestUDPFakeCloseFromReceivedActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper
  var _received: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9806", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    if _received then
      _h.fail("Received data after close")
      _h.complete(false)
    end
    _received = true
    _udp.close()
    KeepReading

  fun ref _on_closed() =>
    _h.assert_true(_received)
    _h.complete(true)

class \nodoc\ iso _TestUDPEcho is UnitTest
  """
  End-to-end UDP echo: one socket sends to another, the other echoes back.
  """
  fun name(): String => "net/UDPEcho"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let server =
      _TestUDPEchoServer(
        UDPAuth(h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        "9807",
        h)
    h.dispose_when_done(server)

actor \nodoc\ _TestUDPEchoServer
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _host: String
  var _client: (_TestUDPEchoClient | None) = None

  new create(auth: UDPAuth,
    host: String,
    port: String,
    h: TestHelper)
  =>
    _h = h
    _host = host
    _udp = UDPSocket(auth, host, port, this, this)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    let addr = _udp.local_address()
    let c = _TestUDPEchoClient(UDPAuth(_h.env.root), _host, addr, _h)
    _client = c

  fun ref _on_bind_failure() =>
    _h.fail("Server bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    _udp.send_to(consume data, from)
    KeepReading

  fun ref _on_closed() =>
    match _client
    | let c: _TestUDPEchoClient =>
      c.dispose()
    end

actor \nodoc\ _TestUDPEchoClient
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _server_addr: NetAddress val

  new create(auth: UDPAuth,
    host: String,
    server_addr: NetAddress val,
    h: TestHelper)
  =>
    _h = h
    _server_addr = server_addr
    _udp = UDPSocket(auth, host, "0", this, this)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.send_to("ping", _server_addr)

  fun ref _on_bind_failure() =>
    _h.fail("Client bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    _h.assert_eq[String]("ping", s)
    _udp.close()
    _h.complete(true)
    KeepReading

class \nodoc\ _FBUDPRecvError is UDPBackend
  """
  Fake backend: bind succeeds, recvfrom always returns Error, sendto returns
  Ok. Used to verify that a per-datagram recvfrom error does not close the
  socket.
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    try
      @pony_asio_event_create(
        the_actor,
        _FakeUDPFd()?,
        AsioEvent.read_write_oneshot(),
        0,
        true)
    else
      AsioEvent.none()
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultError, 0, recover iso NetAddress end)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBUDPRecvAlways is UDPBackend
  """
  Fake backend: bind succeeds, recvfrom always returns a 1-byte datagram.
  Used for testing YieldReading and read-loop budget enforcement.
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    try
      @pony_asio_event_create(
        the_actor,
        _FakeUDPFd()?,
        AsioEvent.read_write_oneshot(),
        0,
        true)
    else
      AsioEvent.none()
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    let msg = "x"
    @memcpy(buffer, msg.cpointer(), msg.size())
    (SocketResultOk, 1, recover iso NetAddress end)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ iso _TestUDPFakeRecvError is UnitTest
  """
  recvfrom returning SocketResultError exits the read loop and defers to
  _read_again. The socket stays open.
  """
  fun name(): String => "net/UDPFakeRecvError"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeRecvErrorActor[_FBUDPRecvError](h)

actor \nodoc\ _TestUDPFakeRecvErrorActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper
  var _read_again_called: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9808", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  be _read_again() =>
    _read_again_called = true
    _h.assert_true(_udp.is_open())
    let dest = recover val NetAddress end
    match \exhaustive\ _udp.send_to("test", dest)
    | SendToOk =>
      _udp.close()
    | let e: SendToFailure =>
      _h.fail("Expected SendToOk after recv error")
      _h.complete(false)
    end

  fun ref _on_bound() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.assert_true(_read_again_called)
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeYieldReading is UnitTest
  """
  Returning YieldReading from _on_received stops the read loop and defers
  to _read_again.
  """
  fun name(): String => "net/UDPFakeYieldReading"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeYieldReadingActor[_FBUDPRecvAlways](h)

actor \nodoc\ _TestUDPFakeYieldReadingActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper
  var _read_again_called: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9809", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  be _read_again() =>
    _read_again_called = true
    _socket().read_again()

  fun ref _on_bound() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    if not _read_again_called then
      YieldReading
    else
      _udp.close()
      KeepReading
    end

  fun ref _on_closed() =>
    _h.assert_true(_read_again_called)
    _h.complete(true)

class \nodoc\ iso _TestUDPFakeBudget is UnitTest
  """
  The datagram-count budget stops the read loop after max_datagrams_per_turn
  datagrams and defers to _read_again.
  """
  fun name(): String => "net/UDPFakeBudget"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPFakeBudgetActor[_FBUDPRecvAlways](h)

actor \nodoc\ _TestUDPFakeBudgetActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper
  var _total_received: USize = 0
  var _turns: USize = 0

  new create(h: TestHelper) =>
    _h = h
    _udp =
      UDPSocket[UDP](
        UDPAuth(_h.env.root), "", "9810", this, this
        where max_datagrams_per_turn = 2)

  fun ref _socket(): UDPSocket[UDP] => _udp

  be _read_again() =>
    _turns = _turns + 1
    _socket().read_again()

  fun ref _on_bound() =>
    _turns = 1
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    _total_received = _total_received + 1
    if _total_received == 4 then
      _h.assert_eq[USize](2, _turns)
      _udp.close()
    end
    KeepReading

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestUDPDisposeInitRace is UnitTest
  """
  If close() arrives before _finish_initialization, no bind attempt is made
  and no lifecycle callbacks fire.
  """
  fun name(): String => "net/UDPDisposeInitRace"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPDisposeInitRaceActor[_FBUDPSendOk](h)

actor \nodoc\ _TestUDPDisposeInitRaceActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper
  var _got_callback: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9811", this, this)
    _udp.close()
    _check_result()

  fun ref _socket(): UDPSocket[UDP] => _udp

  be _check_result() =>
    _h.assert_false(_got_callback)
    _h.assert_true(_udp.is_closed())
    _h.complete(true)

  fun ref _on_bound() =>
    _got_callback = true
    _h.fail("Should not bind after close")
    _h.complete(false)

  fun ref _on_bind_failure() =>
    _got_callback = true
    _h.fail("Should not get bind failure after close")
    _h.complete(false)

class \nodoc\ iso _TestUDPSocketState is UnitTest
  """
  is_open, is_closed, and socket options return correct values across the
  UDP lifecycle.
  """
  fun name(): String => "net/UDPSocketState"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestUDPSocketStateActor[_FBUDPSendOk](h)

actor \nodoc\ _TestUDPSocketStateActor[UDP: UDPBackend ref]
  is (UDPSocketActor[UDP] & UDPLifecycleEventReceiver[UDP])
  var _udp: UDPSocket[UDP] = UDPSocket[UDP].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _h.assert_false(_udp.is_open())
    _h.assert_false(_udp.is_closed())
    (let err_none, _) =
      _udp.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](1, err_none)
    _udp = UDPSocket[UDP](UDPAuth(_h.env.root), "", "9812", this, this)

  fun ref _socket(): UDPSocket[UDP] => _udp

  fun ref _on_bound() =>
    _h.assert_true(_udp.is_open())
    _h.assert_false(_udp.is_closed())
    (let err_open, _) =
      _udp.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](0, err_open)
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.assert_false(_udp.is_open())
    _h.assert_true(_udp.is_closed())
    (let err_closed, _) =
      _udp.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](1, err_closed)
    _h.complete(true)
