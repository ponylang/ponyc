trait ServerLifecycleEventReceiver[TCP: TCPBackend ref = RuntimeBackend]
  """
  Application-level callbacks for server-side TCP connections.
  One receiver per connection, no chaining.
  """
  fun ref _connection(): TCPConnection[TCP]

  fun ref _on_started() =>
    """
    Called when a server connection is ready for application data.
    """
    None

  fun ref _on_closed() =>
    """
    Called when the connection is closed.
    """
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    """
    Called each time data is received on this connection.

    Return `KeepReading` to let the read loop take the next message, or
    `YieldReading` to stop after this one and give other actors a turn.
    """
    KeepReading

  fun ref _on_throttled() =>
    """
    Called when we start experiencing backpressure.

    The sends that completed in the write that hit backpressure are reported
    first, so their `_on_sent` arrives before this. A `hard_close()` from one
    of those callbacks means this one never fires at all: `_on_closed` has
    arrived and the connection is no longer throttled. A graceful `close()`
    leaves the queued writes to drain, so this still fires.
    """
    None

  fun ref _on_unthrottled() =>
    """
    Called when backpressure is released.
    """
    None

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    """
    Called once for each `send()` the connection accepts, from inside that
    `send()` and before the bytes are written. `token` identifies the send and
    comes back exactly once more, to `_on_sent` or `_on_send_failed`. `data`
    is what was passed to `send()`.

    A refused `send()` does not fire this. It returns a `SendError` instead.

    Closing the connection from here does not un-accept the send: `send()`
    still returns `SendAccepted`, and a hard close gives the token to
    `_on_send_failed`.

    Sending from here queues the second send behind the first before either is
    written. Nothing is written between one level of that nesting and the next,
    so unlike the nesting `_on_sent` allows, it has no point at which the
    socket can refuse and stop it. The flush at the innermost level can also
    deliver `_on_sent` for the token this call is still delivering.
    """
    None

  fun ref _on_sent(token: SendToken) =>
    """
    Called when the bytes from an accepted `send()` have been handed to the
    OS: written to the kernel send buffer, not necessarily received by the
    peer. The token matches the one `_on_send_accepted` delivered for that
    send, and this callback fires exactly once for it. Callbacks arrive in
    send order.

    Fires during the write that hands the bytes over, which can be the write
    inside the `send()` call that queued them. So anything the calling code
    updates after that `send()` -- a counter, a map, a flag -- is not updated
    yet when this fires.

    Calling `send()` from here nests on the stack: an application that sends
    its next message on each completion recurses for as long as the sends keep
    draining, and a peer that keeps up sets no bound on the depth. Send from a
    behavior instead to break the nesting.

    A `hard_close()` from here suppresses the `_on_throttled` that backpressure
    was about to deliver.
    """
    None

  fun ref _on_send_failed(token: SendToken) =>
    """
    Called when the bytes from an accepted `send()` could not be handed to
    the OS because the connection was lost or hard-closed first. The token
    matches the one `_on_send_accepted` delivered for that send, and this
    callback fires exactly once for it. A graceful `close()` sends what's still
    queued, so only a hard close or a lost connection fires this. On a hard
    close, the sends that reached the OS fire `_on_sent` and the rest fire
    this, so the split shows how far your data got.

    Always fires in a subsequent behavior turn, never synchronously during
    `hard_close()`. Always arrives after `_on_closed`, which fires
    synchronously during `hard_close()`.
    """
    None

  fun ref _on_start_failure(reason: StartFailureReason)
    """
    Called when a server connection fails to start. This covers failures
    that occur before _on_started would have fired, such as an SSL
    handshake failure. The application was never notified of the connection
    via _on_started.

    The `reason` parameter identifies the cause of the failure.
    `StartFailedSSL` indicates SSL session creation or handshake failure.
    """

  fun ref _on_tls_ready() =>
    """
    Called when a TLS handshake initiated by `start_tls()` completes
    successfully. The connection is now encrypted and ready for
    application data over TLS.
    """
    None

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    """
    Called when a TLS handshake initiated by `start_tls()` fails. Fires
    synchronously during `hard_close()`, immediately before `_on_closed()`.
    The connection was already established (the application received
    `_on_started` earlier), so `_on_closed` always follows to signal
    connection teardown.

    The `reason` parameter distinguishes authentication failures
    (`TLSAuthFailed`) from other protocol errors (`TLSGeneralError`).
    """
    None

  fun ref _on_idle_timeout() =>
    """
    Called when no successful send or receive has occurred for the duration
    configured by `idle_timeout()`. This measures application-level inactivity,
    not wire-level: pending OS write buffer drains and failed sends
    (`SendErrorNotWriteable`) do not count as activity.

    The timer re-arms after each firing while the connection is open. Call
    `idle_timeout(None)` to disable it; `hard_close()` cancels it. A graceful
    `close()` does not, so this can still arrive on a closing connection that is
    moving bytes. The application decides what action to take — close the
    connection, send a keepalive, log a warning, etc.

    If the idle timer's ASIO event subscription fails,
    `_on_idle_timer_failure()` is delivered instead of this callback.
    """
    None

  fun ref _on_idle_timer_failure() =>
    """
    Called when the idle timer's ASIO event subscription fails. This is
    typically caused by the kernel returning an error (e.g. `ENOMEM`) from
    `kevent` or `epoll_ctl` when the runtime tries to register the timer.

    Before this callback fires, the idle timer has already been cancelled:
    the ASIO event is unsubscribed and the configured timeout duration is
    cleared. Idle timeout detection is no longer active on this connection.

    The connection itself is unaffected and continues running. The
    application decides how to recover — for example, call
    `idle_timeout(duration)` from within this callback to re-arm, or
    `close()` the connection. A re-armed timer can itself fail
    asynchronously under sustained pressure; if the new subscription also
    errors, this callback fires again.
    """
    None

  fun ref _on_timer(token: TimerToken) =>
    """
    Called when a one-shot timer created by `set_timer()` fires. The token
    matches the one returned by `set_timer()`.

    Fires once per `set_timer()` call. The timer is consumed before the
    callback, so it is safe to call `set_timer()` from within `_on_timer()`
    to re-arm. No automatic re-arming occurs.

    If the user timer's ASIO event subscription fails, `_on_timer_failure()`
    is delivered instead of this callback.
    """
    None

  fun ref _on_timer_failure() =>
    """
    Called when the user timer's ASIO event subscription fails. This is
    typically caused by the kernel returning an error (e.g. `ENOMEM`) from
    `kevent` or `epoll_ctl` when the runtime tries to register the timer.

    User timers have two error paths:

    - Synchronous: `set_timer()` returns a `SetTimerError`
      (`SetTimerNotOpen` or `SetTimerAlreadyActive`) when preconditions
      prevent the timer from being created at all.
    - Asynchronous: this callback fires when `set_timer()` succeeded but
      the ASIO event subscription later failed.

    Before this callback fires, the user timer has already been cancelled:
    the ASIO event is unsubscribed and the timer token is cleared. The
    token that the application was waiting on is no longer valid.

    The connection itself is unaffected and continues running. The
    application decides how to recover — for example, call
    `set_timer(duration)` from within this callback to create a new timer,
    or `close()` the connection. A new timer can itself fail
    asynchronously under sustained pressure; if the new subscription also
    errors, this callback fires again.
    """
    None
