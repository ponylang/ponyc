use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestIdleTimerResetOnReceive is UnitTest
  """
  Receive resets the idle timer. A fake backend delivers data; the read
  path calls set_timer on the ASIO backend. Assert that set_timer was called
  at least once by the time _on_received fires.
  """
  fun name(): String => "net/IdleTimerResetOnReceive"

  fun apply(h: TestHelper) =>
    h.expect_action("set_timer called")

    let a = _TestIdleTimerResetOnReceiveActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestIdleTimerResetOnReceiveActor
  is (TCPConnectionActor[_FBSendOkRecvHello, _RecordingAsio]
    & ServerLifecycleEventReceiver[_FBSendOkRecvHello, _RecordingAsio])
  var _tcp_connection: TCPConnection[_FBSendOkRecvHello, _RecordingAsio] =
    TCPConnection[_FBSendOkRecvHello, _RecordingAsio].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecvHello, _RecordingAsio].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection()
    : TCPConnection[_FBSendOkRecvHello, _RecordingAsio]
  =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    match \exhaustive\ MakeIdleTimeout(1_000)
    | let t: IdleTimeout =>
      _tcp_connection.idle_timeout(t)
    | let _: ValidationFailure =>
      _h.fail("MakeIdleTimeout(1_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _tcp_connection._asio_ops().set_timer_count == 1 then
      _h.complete_action("set_timer called")
    else
      _h.fail("expected set_timer_count == 1, got " +
        _tcp_connection._asio_ops().set_timer_count.string())
    end
    _tcp_connection.hard_close()
    KeepReading

  be dispose() =>
    _tcp_connection.hard_close()

class \nodoc\ iso _TestIdleTimerResetOnSend is UnitTest
  """
  Send resets the idle timer. A fake backend accepts all bytes; after a
  successful send the connection calls set_timer on the ASIO backend.
  Assert that set_timer was called at least once after the send.
  """
  fun name(): String => "net/IdleTimerResetOnSend"

  fun apply(h: TestHelper) =>
    h.expect_action("set_timer called")

    let a = _TestIdleTimerResetOnSendActor(h)
    h.dispose_when_done(a)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestIdleTimerResetOnSendActor
  is (TCPConnectionActor[_FBSendOkRecvRetry, _RecordingAsio]
    & ServerLifecycleEventReceiver[_FBSendOkRecvRetry, _RecordingAsio])
  var _tcp_connection: TCPConnection[_FBSendOkRecvRetry, _RecordingAsio] =
    TCPConnection[_FBSendOkRecvRetry, _RecordingAsio].none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    try
      let fd = _FakeServerFd()?
      _tcp_connection =
        TCPConnection[_FBSendOkRecvRetry, _RecordingAsio].server(
          TCPServerAuth(_h.env.root),
          fd,
          this,
          this)
    else
      _h.fail("could not allocate socket")
      _h.complete(false)
    end

  fun ref _connection()
    : TCPConnection[_FBSendOkRecvRetry, _RecordingAsio]
  =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    _tcp_connection.mute()
    match \exhaustive\ MakeIdleTimeout(1_000)
    | let t: IdleTimeout =>
      _tcp_connection.idle_timeout(t)
    | let _: ValidationFailure =>
      _h.fail("MakeIdleTimeout(1_000) should succeed")
      _h.complete(false)
      return
    end
    match \exhaustive\ _tcp_connection.send("hello")
    | SendAccepted =>
      if _tcp_connection._asio_ops().set_timer_count == 1 then
        _h.complete_action("set_timer called")
      else
        _h.fail("expected set_timer_count == 1, got " +
          _tcp_connection._asio_ops().set_timer_count.string())
      end
    | let _: SendError =>
      _h.fail("send returned an error")
    end
    _tcp_connection.hard_close()

  be dispose() =>
    _tcp_connection.hard_close()
