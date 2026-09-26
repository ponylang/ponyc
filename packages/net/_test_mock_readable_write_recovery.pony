use "pony_test"

class \nodoc\ _FAReadableWhileThrottled is AsioBackend
  """
  Simulates write-interest recovery after a readable event while throttled.

  On non-BSD (epoll, Windows), a readable event disarms the whole-fd
  subscription including writes. The trailing guard in `_dispatch_io_event`
  re-arms write interest via `resubscribe_write`; this mock injects a
  writeable event when that call arrives after the readable was processed.

  On BSD (kqueue), read and write are independent filters — a readable event
  never disarms write interest. This mock injects the writeable event from
  `resubscribe_read` (which fires after `_read` exits), simulating a write
  notification that was never lost.
  """
  var _target: (_AsioEventInjectable tag | None) = None
  var _main_event: AsioEventID = AsioEvent.none()
  var _timer_event: AsioEventID = AsioEvent.none()
  var _fd: U32 = 0
  var _main_disposed: Bool = false
  var _timer_disposed: Bool = false
  var _readable_processed: Bool = false

  new create() => None

  fun ref set_target(target: _AsioEventInjectable tag) =>
    _target = target

  fun main_event(): AsioEventID => _main_event

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

  fun ref resubscribe_read(event: AsioEventID) =>
    _readable_processed = true
    ifdef bsd then
      match _target
      | let t: _AsioEventInjectable tag =>
        t._inject_asio_event(_main_event, AsioEvent.write())
      end
    end

  fun ref resubscribe_write(event: AsioEventID) =>
    ifdef not bsd then
      if _readable_processed then
        match _target
        | let t: _AsioEventInjectable tag =>
          t._inject_asio_event(event, AsioEvent.write())
        end
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

class \nodoc\ iso _TestMockReadableEventWriteRecovery is UnitTest
  """
  Verify that a connection recovers write interest after a readable event
  arrives while throttled.

  Sequence:
  1. Server starts, sends data — sendv returns Retry, throttle fires.
  2. Test injects a readable event while throttled.
  3. receive returns Retry (EAGAIN), _read exits.
  4. The mock injects a writeable event once write interest is restored.
  5. _set_writeable → _release_backpressure → _on_unthrottled.

  The recovery mechanism differs by platform. On epoll/Windows, a readable
  event disarms the whole-fd subscription; the trailing guard in
  `_dispatch_io_event` re-arms writes via `resubscribe_write`. On kqueue,
  read and write are independent filters, so write interest is never lost.
  The mock simulates each platform's behavior; see `_FAReadableWhileThrottled`.

  Timeout means the write interest was never recovered.
  """
  fun name(): String => "net/mock/ReadableEventWriteRecovery"

  fun ref apply(h: TestHelper) =>
    h.expect_action("started")
    h.expect_action("throttled")
    h.expect_action("unthrottled")
    h.expect_action("closed")

    let a = _TestMockReadableWriteRecoveryActor(h)
    h.dispose_when_done(a)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestMockReadableWriteRecoveryActor
  is (TCPConnectionActor[_FBBackpressureRetry, _FAReadableWhileThrottled]
    & ServerLifecycleEventReceiver[_FBBackpressureRetry,
        _FAReadableWhileThrottled]
    & _AsioEventInjectable)
  var _tcp_connection:
    TCPConnection[_FBBackpressureRetry, _FAReadableWhileThrottled] =
    TCPConnection[_FBBackpressureRetry, _FAReadableWhileThrottled].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBBackpressureRetry, _FAReadableWhileThrottled].server(
        TCPServerAuth(_h.env.root),
        _SyntheticFd(),
        this,
        this)
    _tcp_connection._asio_ops().set_target(this)

  fun ref _connection():
    TCPConnection[_FBBackpressureRetry, _FAReadableWhileThrottled]
  =>
    _tcp_connection

  be _inject_asio_event(event: AsioEventID, flags: U32) =>
    _connection()._event_notify(event, flags)

  fun ref _on_start_failure(reason: StartFailureReason) => None

  fun ref _on_started() =>
    _h.complete_action("started")
    _tcp_connection.send("hello")

  fun ref _on_throttled() =>
    _h.complete_action("throttled")
    _inject_readable()

  be _inject_readable() =>
    let event = _tcp_connection._asio_ops().main_event()
    _connection()._event_notify(event, AsioEvent.read())

  fun ref _on_unthrottled() =>
    _h.complete_action("unthrottled")
    _tcp_connection.hard_close()

  fun ref _on_closed() =>
    _h.complete_action("closed")
    _h.complete(true)

  be dispose() =>
    _tcp_connection.hard_close()
