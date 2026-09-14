use "pony_test"

class \nodoc\ _FAServerDisconnect is AsioBackend
  var _target: (_AsioEventInjectable tag | None) = None
  var _main_event: AsioEventID = AsioEvent.none()
  var _timer_event: AsioEventID = AsioEvent.none()
  var _fd: U32 = 0
  var _main_disposed: Bool = false
  var _timer_disposed: Bool = false

  new create() => None

  fun ref set_target(target: _AsioEventInjectable tag) =>
    _target = target

  fun ref create_event(the_actor: AsioEventNotify, fd: U32): AsioEventID =>
    _fd = fd
    _main_event =
      _SyntheticEvent()
    _main_event

  fun ref destroy(event: AsioEventID) => None

  fun ref unsubscribe(event: AsioEventID) =>
    if event is _main_event then
      _main_disposed = true
      match _target
      | let t: _AsioEventInjectable tag =>
        t._inject_asio_event(event, AsioEvent.dispose())
      end
    elseif event is _timer_event then
      _timer_disposed = true
      match _target
      | let t: _AsioEventInjectable tag =>
        t._inject_asio_event(event, AsioEvent.dispose())
      end
    end

  fun ref resubscribe_read(event: AsioEventID) => None

  fun ref resubscribe_write(event: AsioEventID) => None

  fun ref create_timer_event(the_actor: AsioEventNotify, nsec: U64)
    : AsioEventID
  =>
    _timer_event =
      _SyntheticEvent()
    _timer_event

  fun ref set_timer(event: AsioEventID, nsec: U64) => None
  fun ref set_readable(event: AsioEventID) => None
  fun ref set_unreadable(event: AsioEventID) => None
  fun ref set_writeable(event: AsioEventID) => None
  fun ref set_unwriteable(event: AsioEventID) => None

  fun ref event_fd(event: AsioEventID): U32 => _fd

  fun ref get_disposable(event: AsioEventID): Bool =>
    if event is _main_event then
      _main_disposed
    elseif event is _timer_event then
      _timer_disposed
    else
      false
    end

class \nodoc\ _FBRecvError is TCPBackend
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) =>
    if fd != _SyntheticFd() then
      @pony_os_socket_close(fd)
    end

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None
  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false
  fun ref shutdown(fd: U32) => None
  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun is_socket_connected(fd: U32): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultError, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total =
        total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FADisconnectDuringBackpressure is AsioBackend
  var _target: (_AsioEventInjectable tag | None) = None
  var _main_event: AsioEventID = AsioEvent.none()
  var _timer_event: AsioEventID = AsioEvent.none()
  var _fd: U32 = 0
  var _main_disposed: Bool = false
  var _timer_disposed: Bool = false
  var _write_injected: Bool = false

  new create() => None

  fun ref set_target(target: _AsioEventInjectable tag) =>
    _target = target

  fun ref create_event(the_actor: AsioEventNotify, fd: U32): AsioEventID =>
    _fd = fd
    _main_event =
      _SyntheticEvent()
    _main_event

  fun ref destroy(event: AsioEventID) => None

  fun ref unsubscribe(event: AsioEventID) =>
    if event is _main_event then
      _main_disposed = true
      match _target
      | let t: _AsioEventInjectable tag =>
        t._inject_asio_event(event, AsioEvent.dispose())
      end
    elseif event is _timer_event then
      _timer_disposed = true
      match _target
      | let t: _AsioEventInjectable tag =>
        t._inject_asio_event(event, AsioEvent.dispose())
      end
    end

  fun ref resubscribe_read(event: AsioEventID) => None

  fun ref resubscribe_write(event: AsioEventID) =>
    if not _write_injected then
      _write_injected = true
      match _target
      | let t: _AsioEventInjectable tag =>
        t._inject_asio_event(event, _AsioErrorFlag())
      end
    end

  fun ref create_timer_event(the_actor: AsioEventNotify, nsec: U64)
    : AsioEventID
  =>
    _timer_event =
      _SyntheticEvent()
    _timer_event

  fun ref set_timer(event: AsioEventID, nsec: U64) => None
  fun ref set_readable(event: AsioEventID) => None
  fun ref set_unreadable(event: AsioEventID) => None
  fun ref set_writeable(event: AsioEventID) => None
  fun ref set_unwriteable(event: AsioEventID) => None

  fun ref event_fd(event: AsioEventID): U32 => _fd

  fun ref get_disposable(event: AsioEventID): Bool =>
    if event is _main_event then
      _main_disposed
    elseif event is _timer_event then
      _timer_disposed
    else
      false
    end

class \nodoc\ iso _TestServerDisconnect is UnitTest
  fun name(): String => "net/mock/ServerDisconnect"

  fun apply(h: TestHelper) =>
    h.expect_action("started")
    h.expect_action("closed")

    let a = _TestServerDisconnectActor(h)
    h.dispose_when_done(a)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestServerDisconnectActor
  is (TCPConnectionActor[_FBRecvError, _FAServerDisconnect]
    & ServerLifecycleEventReceiver[_FBRecvError, _FAServerDisconnect]
    & _AsioEventInjectable)
  var _tcp_connection:
    TCPConnection[_FBRecvError, _FAServerDisconnect] =
    TCPConnection[_FBRecvError, _FAServerDisconnect].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBRecvError, _FAServerDisconnect].server(
        TCPServerAuth(_h.env.root),
        _SyntheticFd(),
        this,
        this)
    _tcp_connection._asio_ops().set_target(this)

  fun ref _connection():
    TCPConnection[_FBRecvError, _FAServerDisconnect]
  =>
    _tcp_connection

  be _inject_asio_event(event: AsioEventID, flags: U32) =>
    _connection()._event_notify(event, flags)

  fun ref _on_start_failure(reason: StartFailureReason) => None

  fun ref _on_started() =>
    _h.complete_action("started")

  fun ref _on_closed() =>
    _h.complete_action("closed")

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestDisconnectDuringBackpressure is UnitTest
  fun name(): String => "net/mock/DisconnectDuringBackpressure"

  fun apply(h: TestHelper) =>
    h.expect_action("started")
    h.expect_action("throttled")
    h.expect_action("closed")

    let a = _TestDisconnectDuringBackpressureActor(h)
    h.dispose_when_done(a)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestDisconnectDuringBackpressureActor
  is (TCPConnectionActor[_FBBackpressureRetry,
        _FADisconnectDuringBackpressure]
    & ServerLifecycleEventReceiver[_FBBackpressureRetry,
        _FADisconnectDuringBackpressure]
    & _AsioEventInjectable)
  var _tcp_connection:
    TCPConnection[_FBBackpressureRetry,
      _FADisconnectDuringBackpressure] =
    TCPConnection[_FBBackpressureRetry,
      _FADisconnectDuringBackpressure].none()
  let _h: TestHelper
  var _got_unthrottled: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBBackpressureRetry,
        _FADisconnectDuringBackpressure].server(
        TCPServerAuth(_h.env.root),
        _SyntheticFd(),
        this,
        this)
    _tcp_connection._asio_ops().set_target(this)

  fun ref _connection():
    TCPConnection[_FBBackpressureRetry,
      _FADisconnectDuringBackpressure]
  =>
    _tcp_connection

  be _inject_asio_event(event: AsioEventID, flags: U32) =>
    _connection()._event_notify(event, flags)

  fun ref _on_start_failure(reason: StartFailureReason) => None

  fun ref _on_started() =>
    _h.complete_action("started")
    _tcp_connection.send("hello world")

  fun ref _on_throttled() =>
    _h.complete_action("throttled")

  fun ref _on_unthrottled() =>
    _got_unthrottled = true
    _h.fail("unexpected _on_unthrottled")

  fun ref _on_closed() =>
    _h.assert_false(
      _got_unthrottled,
      "_on_unthrottled should not fire before _on_closed")
    _h.complete_action("closed")

  be dispose() =>
    _tcp_connection.hard_close()
