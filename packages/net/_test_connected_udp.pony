use "pony_test"

class \nodoc\ iso _TestConnectedUDPEcho is UnitTest
  """
  A connected UDP socket sends to an unconnected echo server, which replies
  via sendto. The connected socket receives the echo (the kernel filters by
  source address).
  """
  fun name(): String => "net/ConnectedUDPEcho"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let server =
      _TestCUDPEchoServer(
        UDPAuth(h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        h)
    h.dispose_when_done(server)

actor \nodoc\ _TestCUDPEchoServer
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _host: String
  var _client: (_TestCUDPEchoClient | None) = None

  new create(auth: UDPAuth, host: String, h: TestHelper) =>
    _h = h
    _host = host
    _udp = UDPSocket(auth, host, "0", this, this)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    let addr = _udp.local_address()
    let c = _TestCUDPEchoClient(UDPAuth(_h.env.root), _host, addr, _h)
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
    | let c: _TestCUDPEchoClient => c.dispose()
    end

actor \nodoc\ _TestCUDPEchoClient
  is (ConnectedUDPSocketActor & ConnectedUDPLifecycleEventReceiver)
  var _udp: ConnectedUDPSocket = ConnectedUDPSocket.none()
  let _h: TestHelper

  new create(auth: UDPAuth,
    host: String,
    server_addr: NetAddress val,
    h: TestHelper)
  =>
    _h = h
    (let peer_host, let peer_port) =
      try
        server_addr.name()?
      else
        _h.fail("Cannot resolve server address")
        _h.complete(false)
        return
      end
    _udp =
      ConnectedUDPSocket(auth, host, "0", peer_host, peer_port, this, this)

  fun ref _socket(): ConnectedUDPSocket => _udp

  fun ref _on_connected() =>
    _udp.send("ping")

  fun ref _on_bind_failure() =>
    _h.fail("Client bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Client connect failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let s = String.from_array(consume data)
    _h.assert_eq[String]("ping", s)
    _udp.close()
    _h.complete(true)
    KeepReading

class \nodoc\ iso _TestConnectedUDPRemoteAddress is UnitTest
  """
  After connecting, `remote_address` returns the peer's address.
  """
  fun name(): String => "net/ConnectedUDPRemoteAddress"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let server =
      _TestCUDPRemoteAddrServer(
        UDPAuth(h.env.root),
        ifdef linux then "127.0.0.2" else "localhost" end,
        h)
    h.dispose_when_done(server)

actor \nodoc\ _TestCUDPRemoteAddrServer
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _host: String
  var _client: (_TestCUDPRemoteAddrClient | None) = None

  new create(auth: UDPAuth, host: String, h: TestHelper) =>
    _h = h
    _host = host
    _udp = UDPSocket(auth, host, "0", this, this)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    let c =
      _TestCUDPRemoteAddrClient(
        UDPAuth(_h.env.root), _host, _udp.local_address(), _h)
    _client = c

  fun ref _on_bind_failure() =>
    _h.fail("Server bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    match _client
    | let c: _TestCUDPRemoteAddrClient => c.dispose()
    end

actor \nodoc\ _TestCUDPRemoteAddrClient
  is (ConnectedUDPSocketActor & ConnectedUDPLifecycleEventReceiver)
  var _udp: ConnectedUDPSocket = ConnectedUDPSocket.none()
  let _h: TestHelper
  let _server_addr: NetAddress val

  new create(auth: UDPAuth,
    host: String,
    server_addr: NetAddress val,
    h: TestHelper)
  =>
    _h = h
    _server_addr = server_addr
    (let peer_host, let peer_port) =
      try
        server_addr.name()?
      else
        _h.fail("Cannot resolve server address")
        _h.complete(false)
        return
      end
    _udp =
      ConnectedUDPSocket(auth, host, "0", peer_host, peer_port, this, this)

  fun ref _socket(): ConnectedUDPSocket => _udp

  fun ref _on_connected() =>
    let remote = _udp.remote_address()
    _h.assert_eq[U16](_server_addr.port(), remote.port())
    _udp.close()
    _h.complete(true)

  fun ref _on_bind_failure() =>
    _h.fail("Client bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Client connect failed")
    _h.complete(false)

class \nodoc\ iso _TestConnectedUDPConnectFailure is UnitTest
  """
  Connecting to an unresolvable host fires `_on_connect_failure`.
  """
  fun name(): String => "net/ConnectedUDPConnectFailure"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _TestCUDPConnectFail(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _TestCUDPConnectFail
  is (ConnectedUDPSocketActor & ConnectedUDPLifecycleEventReceiver)
  var _udp: ConnectedUDPSocket = ConnectedUDPSocket.none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp =
      ConnectedUDPSocket(
        auth,
        host,
        "0",
        "this.host.does.not.exist.invalid",
        "9",
        this,
        this)

  fun ref _socket(): ConnectedUDPSocket => _udp

  fun ref _on_connected() =>
    _h.fail("Should not connect to unresolvable host")
    _h.complete(false)

  fun ref _on_bind_failure() =>
    _h.fail("Bind should succeed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.complete(true)

class \nodoc\ _FBConnectedUDPOk is ConnectedUDPBackend
  """
  Fake backend: bind and connect succeed, send returns Ok, recvfrom returns
  retry.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    true

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultRetry, 0, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPConnectFail is ConnectedUDPBackend
  """
  Fake backend: bind succeeds, connect fails.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    false

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultError, 0, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultError

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPBindFail is ConnectedUDPBackend
  """
  Fake backend: bind fails.
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    false

  fun ref close(fd: U32) => None

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultError, 0, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultError

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPSendWouldBlock is ConnectedUDPBackend
  """
  Fake backend: bind and connect succeed, send returns Retry.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    true

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultRetry, 0, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultRetry

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPSendError is ConnectedUDPBackend
  """
  Fake backend: bind and connect succeed, send returns Error.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    true

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultRetry, 0, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultError

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPRecvHello is ConnectedUDPBackend
  """
  Fake backend: bind and connect succeed, first recvfrom delivers "hello",
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    true

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

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPRecvError is ConnectedUDPBackend
  """
  Fake backend: bind and connect succeed, recvfrom always returns Error,
  send returns Ok.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    true

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    (SocketResultError, 0, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ _FBConnectedUDPRecvAlways is ConnectedUDPBackend
  """
  Fake backend: bind and connect succeed, recvfrom always returns a 1-byte
  datagram.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
  =>
    true

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    let msg = "x"
    @memcpy(buffer, msg.cpointer(), msg.size())
    (SocketResultOk, 1, recover iso NetAddress end)

  fun ref send(fd: U32, data: ByteSeq): SocketResult =>
    SocketResultOk

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

class \nodoc\ iso _TestConnectedUDPRecvData is UnitTest
  """
  _on_received delivers data from recvfrom.
  """
  fun name(): String => "net/ConnectedUDPRecvData"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let a =
      _FBCUDPRecvDataActor[_FBConnectedUDPRecvHello](UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPRecvDataActor[UDP: ConnectedUDPBackend ref]
  is (ConnectedUDPSocketActor[UDP]
    & ConnectedUDPLifecycleEventReceiver[UDP])
  var _udp: ConnectedUDPSocket[UDP] = ConnectedUDPSocket[UDP].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[UDP](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[UDP] => _udp

  fun ref _on_connected() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Connect failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let s = String.from_array(consume data)
    _h.assert_eq[String]("hello", s)
    _udp.close()
    KeepReading

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestConnectedUDPRecvError is UnitTest
  """
  recvfrom returning SocketResultError exits the read loop and defers to
  _read_again. The socket stays open.
  """
  fun name(): String => "net/ConnectedUDPRecvError"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let a =
      _FBCUDPRecvErrorActor[_FBConnectedUDPRecvError](
        UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPRecvErrorActor[UDP: ConnectedUDPBackend ref]
  is (ConnectedUDPSocketActor[UDP]
    & ConnectedUDPLifecycleEventReceiver[UDP])
  var _udp: ConnectedUDPSocket[UDP] = ConnectedUDPSocket[UDP].none()
  let _h: TestHelper
  var _read_again_called: Bool = false

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[UDP](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[UDP] => _udp

  be _read_again() =>
    _read_again_called = true
    _h.assert_true(_udp.is_open())
    _udp.close()

  fun ref _on_connected() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Connect failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.assert_true(_read_again_called)
    _h.complete(true)

class \nodoc\ iso _TestConnectedUDPCloseFromReceived is UnitTest
  """
  Closing from _on_received stops the read loop immediately.
  """
  fun name(): String => "net/ConnectedUDPCloseFromReceived"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let a =
      _FBCUDPCloseFromRecvActor[_FBConnectedUDPRecvAlways](
        UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPCloseFromRecvActor[UDP: ConnectedUDPBackend ref]
  is (ConnectedUDPSocketActor[UDP]
    & ConnectedUDPLifecycleEventReceiver[UDP])
  var _udp: ConnectedUDPSocket[UDP] = ConnectedUDPSocket[UDP].none()
  let _h: TestHelper
  var _received: Bool = false

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[UDP](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[UDP] => _udp

  fun ref _on_connected() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Connect failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
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

class \nodoc\ iso _TestConnectedUDPYieldReading is UnitTest
  """
  Returning YieldReading from _on_received stops the read loop and defers
  to _read_again.
  """
  fun name(): String => "net/ConnectedUDPYieldReading"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let a =
      _FBCUDPYieldReadingActor[_FBConnectedUDPRecvAlways](
        UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPYieldReadingActor[UDP: ConnectedUDPBackend ref]
  is (ConnectedUDPSocketActor[UDP]
    & ConnectedUDPLifecycleEventReceiver[UDP])
  var _udp: ConnectedUDPSocket[UDP] = ConnectedUDPSocket[UDP].none()
  let _h: TestHelper
  var _read_again_called: Bool = false

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[UDP](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[UDP] => _udp

  be _read_again() =>
    _read_again_called = true
    _socket().read_again()

  fun ref _on_connected() =>
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Connect failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if not _read_again_called then
      YieldReading
    else
      _udp.close()
      KeepReading
    end

  fun ref _on_closed() =>
    _h.assert_true(_read_again_called)
    _h.complete(true)

class \nodoc\ iso _TestConnectedUDPBudget is UnitTest
  """
  The datagram-count budget stops the read loop after max_datagrams_per_turn
  datagrams and defers to _read_again.
  """
  fun name(): String => "net/ConnectedUDPBudget"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let a =
      _FBCUDPBudgetActor[_FBConnectedUDPRecvAlways](UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPBudgetActor[UDP: ConnectedUDPBackend ref]
  is (ConnectedUDPSocketActor[UDP]
    & ConnectedUDPLifecycleEventReceiver[UDP])
  var _udp: ConnectedUDPSocket[UDP] = ConnectedUDPSocket[UDP].none()
  let _h: TestHelper
  var _total_received: USize = 0
  var _turns: USize = 0

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[UDP](
        auth, "", "0", "localhost", "12345", this, this
        where max_datagrams_per_turn = 2)

  fun ref _socket(): ConnectedUDPSocket[UDP] => _udp

  be _read_again() =>
    _turns = _turns + 1
    _socket().read_again()

  fun ref _on_connected() =>
    _turns = 1
    _udp.read_again()

  fun ref _on_bind_failure() =>
    _h.fail("Bind failed")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Connect failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _total_received = _total_received + 1
    if _total_received == 4 then
      _h.assert_eq[USize](2, _turns)
      _udp.close()
    end
    KeepReading

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestConnectedUDPDisposeInitRace is UnitTest
  """
  If close() arrives before _finish_initialization, no bind attempt is made
  and no lifecycle callbacks fire.
  """
  fun name(): String => "net/ConnectedUDPDisposeInitRace"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _FBCUDPDisposeInitRaceActor[_FBConnectedUDPOk](UDPAuth(h.env.root), h)

actor \nodoc\ _FBCUDPDisposeInitRaceActor[UDP: ConnectedUDPBackend ref]
  is (ConnectedUDPSocketActor[UDP]
    & ConnectedUDPLifecycleEventReceiver[UDP])
  var _udp: ConnectedUDPSocket[UDP] = ConnectedUDPSocket[UDP].none()
  let _h: TestHelper
  var _got_callback: Bool = false

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[UDP](
        auth, "", "0", "localhost", "12345", this, this)
    _udp.close()
    _check_result()

  fun ref _socket(): ConnectedUDPSocket[UDP] => _udp

  be _check_result() =>
    _h.assert_false(_got_callback)
    _h.assert_true(_udp.is_closed())
    _h.complete(true)

  fun ref _on_connected() =>
    _got_callback = true
    _h.fail("Should not connect after close")
    _h.complete(false)

  fun ref _on_bind_failure() =>
    _got_callback = true
    _h.fail("Should not get bind failure after close")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _got_callback = true
    _h.fail("Should not get connect failure after close")
    _h.complete(false)

class \nodoc\ iso _TestConnectedUDPSendOk is UnitTest
  fun name(): String => "net/ConnectedUDPSendOk"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPSendOkActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPSendOkActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPOk]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPOk])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPOk] =
    ConnectedUDPSocket[_FBConnectedUDPOk].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPOk](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPOk] => _udp

  fun ref _on_connected() =>
    match _udp.send("hello")
    | UDPSendOk => _h.complete(true)
    else
      _h.fail("Expected UDPSendOk")
      _h.complete(false)
    end
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Unexpected bind failure")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Unexpected connect failure")
    _h.complete(false)

class \nodoc\ iso _TestConnectedUDPSendWouldBlock is UnitTest
  fun name(): String => "net/ConnectedUDPSendWouldBlock"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPSendWouldBlockActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPSendWouldBlockActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPSendWouldBlock]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPSendWouldBlock])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPSendWouldBlock] =
    ConnectedUDPSocket[_FBConnectedUDPSendWouldBlock].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPSendWouldBlock](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPSendWouldBlock] =>
    _udp

  fun ref _on_connected() =>
    match _udp.send("hello")
    | UDPSendWouldBlock => _h.complete(true)
    else
      _h.fail("Expected UDPSendWouldBlock")
      _h.complete(false)
    end
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Unexpected bind failure")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Unexpected connect failure")
    _h.complete(false)

class \nodoc\ iso _TestConnectedUDPSendErr is UnitTest
  fun name(): String => "net/ConnectedUDPSendError"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPSendErrorActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPSendErrorActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPSendError]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPSendError])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPSendError] =
    ConnectedUDPSocket[_FBConnectedUDPSendError].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPSendError](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPSendError] => _udp

  fun ref _on_connected() =>
    match _udp.send("hello")
    | UDPSendError => _h.complete(true)
    else
      _h.fail("Expected UDPSendError")
      _h.complete(false)
    end
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Unexpected bind failure")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Unexpected connect failure")
    _h.complete(false)

class \nodoc\ iso _TestConnectedUDPSendNotOpen is UnitTest
  """
  send returns UDPSendNotOpen when the socket has been closed.
  """
  fun name(): String => "net/ConnectedUDPSendNotOpen"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPSendNotOpenActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPSendNotOpenActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPOk]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPOk])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPOk] =
    ConnectedUDPSocket[_FBConnectedUDPOk].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPOk](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPOk] => _udp

  fun ref _on_connected() =>
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Unexpected bind failure")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Unexpected connect failure")
    _h.complete(false)

  fun ref _on_closed() =>
    match _udp.send("hello")
    | UDPSendNotOpen => _h.complete(true)
    else
      _h.fail("Expected UDPSendNotOpen")
      _h.complete(false)
    end

class \nodoc\ iso _TestConnectedUDPBindFail is UnitTest
  fun name(): String => "net/ConnectedUDPBindFail"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPBindFailActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPBindFailActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPBindFail]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPBindFail])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPBindFail] =
    ConnectedUDPSocket[_FBConnectedUDPBindFail].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPBindFail](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPBindFail] => _udp
  fun ref _on_connected() => None

  fun ref _on_bind_failure() =>
    _h.complete(true)

  fun ref _on_connect_failure() =>
    _h.fail("Unexpected connect failure")
    _h.complete(false)

class \nodoc\ iso _TestConnectedUDPConnectFail is UnitTest
  fun name(): String => "net/ConnectedUDPConnectFail"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPConnectFailActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPConnectFailActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPConnectFail]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPConnectFail])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPConnectFail] =
    ConnectedUDPSocket[_FBConnectedUDPConnectFail].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPConnectFail](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPConnectFail] => _udp
  fun ref _on_connected() => None

  fun ref _on_bind_failure() =>
    _h.fail("Unexpected bind failure")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.complete(true)

class \nodoc\ iso _TestConnectedUDPSocketState is UnitTest
  """
  is_open, is_closed, and socket options return correct values across the
  connected UDP lifecycle.
  """
  fun name(): String => "net/ConnectedUDPSocketState"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let a = _FBCUDPSocketStateActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(a)

actor \nodoc\ _FBCUDPSocketStateActor
  is (ConnectedUDPSocketActor[_FBConnectedUDPOk]
    & ConnectedUDPLifecycleEventReceiver[_FBConnectedUDPOk])
  var _udp: ConnectedUDPSocket[_FBConnectedUDPOk] =
    ConnectedUDPSocket[_FBConnectedUDPOk].none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _h.assert_false(_udp.is_open())
    _h.assert_false(_udp.is_closed())
    (let err_none, _) =
      _udp.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](1, err_none, "getsockopt should fail in None state")
    _udp =
      ConnectedUDPSocket[_FBConnectedUDPOk](
        auth, "", "0", "localhost", "12345", this, this)

  fun ref _socket(): ConnectedUDPSocket[_FBConnectedUDPOk] => _udp

  fun ref _on_connected() =>
    _h.assert_true(_udp.is_open(), "should be open after connect")
    _h.assert_false(_udp.is_closed(), "should not be closed after connect")
    (let err_open, _) =
      _udp.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](0, err_open, "getsockopt should succeed when open")
    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("Unexpected bind failure")
    _h.complete(false)

  fun ref _on_connect_failure() =>
    _h.fail("Unexpected connect failure")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.assert_false(_udp.is_open(), "should not be open after close")
    _h.assert_true(_udp.is_closed(), "should be closed after close")
    (let err_closed, _) =
      _udp.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](
      1, err_closed, "getsockopt should fail when closed")
    _h.complete(true)
