use "pony_test"

class \nodoc\ _FBPartialThenPartial is TCPBackend
  """
  sendv: first call is a partial write; second call completes the first
  send's remaining bytes but leaves the second send pending. receive:
  always retries.
  """
  var _step: USize = 0

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
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      (SocketResultOk, 1)
    else
      (SocketResultOk, 2)
    end

class \nodoc\ iso _TestSendDeliveredNotFailedOnHardClose is UnitTest
  """
  When hard_close fires mid-flush, sends whose bytes already reached the
  OS get _on_sent, not _on_send_failed. Sends whose bytes have not
  reached the OS get _on_send_failed.

  The scenario:
  1. Send "abc" (3 bytes). sendv accepts 1 byte — partial write,
     backpressure, throttle.
  2. Writeable event fires. _set_writeable → _release_backpressure →
     _on_unthrottled.
  3. _on_unthrottled sends "xyz" (3 bytes). Two tokens are now in the
     pending queue alongside 2 leftover bytes from "abc".
  4. sendv accepts 2 bytes — the remaining "abc" bytes. Token 1
     (offset 3) completes; token 2 (offset 6) does not. Partial write,
     backpressure again.
  5. _apply_backpressure → _fire_completed_sends fires _on_sent for
     token 1 → _on_throttled → hard_close → token 2 gets
     _on_send_failed (deferred).
  """
  fun name(): String => "net/mock/SendDeliveredNotFailedOnHardClose"

  fun apply(h: TestHelper) =>
    h.expect_action("started")
    h.expect_action("throttled")
    h.expect_action("unthrottled")
    h.expect_action("sent")
    h.expect_action("throttled_again")
    h.expect_action("closed")
    h.expect_action("send_failed")

    let a = _TestSendDeliveredActor(h)
    h.dispose_when_done(a)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestSendDeliveredActor
  is (TCPConnectionActor[_FBPartialThenPartial, _FABackpressureRetry]
    & ServerLifecycleEventReceiver[_FBPartialThenPartial,
        _FABackpressureRetry]
    & _AsioEventInjectable)
  var _tcp_connection:
    TCPConnection[_FBPartialThenPartial, _FABackpressureRetry] =
    TCPConnection[_FBPartialThenPartial, _FABackpressureRetry].none()
  let _h: TestHelper
  var _throttle_count: USize = 0
  var _closed_first: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection[_FBPartialThenPartial, _FABackpressureRetry].server(
        TCPServerAuth(_h.env.root),
        _SyntheticFd(),
        this,
        this)
    _tcp_connection._asio_ops().set_target(this)

  fun ref _connection():
    TCPConnection[_FBPartialThenPartial, _FABackpressureRetry]
  =>
    _tcp_connection

  be _inject_asio_event(event: AsioEventID, flags: U32) =>
    _connection()._event_notify(event, flags)

  fun ref _on_start_failure(reason: StartFailureReason) => None

  fun ref _on_started() =>
    _h.complete_action("started")
    _tcp_connection.send("abc")

  fun ref _on_throttled() =>
    _throttle_count = _throttle_count + 1
    if _throttle_count == 1 then
      _h.complete_action("throttled")
    elseif _throttle_count == 2 then
      _h.complete_action("throttled_again")
      _tcp_connection.hard_close()
    else
      _h.fail("unexpected throttle #" + _throttle_count.string())
    end

  fun ref _on_unthrottled() =>
    _h.complete_action("unthrottled")
    _tcp_connection.send("xyz")

  fun ref _on_sent(token: SendToken) =>
    _h.assert_false(
      _closed_first,
      "_on_sent must precede _on_closed")
    _h.complete_action("sent")

  fun ref _on_send_failed(token: SendToken) =>
    _h.assert_true(
      _closed_first,
      "_on_closed must precede _on_send_failed")
    _h.complete_action("send_failed")

  fun ref _on_closed() =>
    _closed_first = true
    _h.complete_action("closed")

  be dispose() =>
    _tcp_connection.hard_close()
