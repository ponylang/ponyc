use net = ".."

trait ServerTCPConnectionNotify
  """
  Callbacks for a server-side TCP connection. Implement this trait and return
  it from `TCPListenNotify.connected()` to handle events on an accepted
  connection.

  All callbacks receive the connection as `conn ref`, so synchronous methods
  on the connection (socket options, `buffer_until`, `close`, etc.) are
  callable from any callback. Behaviors like `write` are also callable but
  execute on the next turn.

  Every callback has a default implementation except `on_start_failure`,
  which must be implemented because ignoring a server-side start failure
  is never correct.
  """
  fun ref on_accepted(conn: ServerTCPConnection ref) =>
    """
    Called when a server connection is ready for application data. For SSL
    connections, this fires after the TLS handshake completes.

    Server connections start with the default read buffer size (16 KiB).
    Call `conn.resize_read_buffer()` here to use a different size.
    """
    None

  fun ref on_received(conn: ServerTCPConnection ref, data: Array[U8] iso):
    net.ReadAction
  =>
    """
    Called when data arrives on the connection. Return `KeepReading` to
    continue reading or `YieldReading` to stop after this message and give
    other actors a turn.
    """
    net.KeepReading

  fun ref on_send_accepted(conn: ServerTCPConnection ref,
    token: net.SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    """
    Called when a `write` or `writev` is accepted. Because `write` and `writev`
    are behaviors, this fires in their behavior turn, not during the callback
    that called `write`. `token` identifies this send and arrives exactly once
    more at `on_sent` or `on_send_failed`.
    """
    None

  fun ref on_sent(conn: ServerTCPConnection ref, token: net.SendToken) =>
    """
    Called when the bytes from an accepted send have been handed to the OS.
    The token matches the one delivered by `on_send_accepted`.
    """
    None

  fun ref on_send_failed(conn: ServerTCPConnection ref,
    token: net.SendToken)
  =>
    """
    Called when an accepted send's bytes could not reach the OS because the
    connection was lost or hard-closed first. Arrives after `on_closed`.
    """
    None

  fun ref on_throttled(conn: ServerTCPConnection ref) =>
    """
    Called when the connection applies backpressure. `write` calls after this
    are silently dropped until `on_unthrottled` fires.
    """
    None

  fun ref on_unthrottled(conn: ServerTCPConnection ref) =>
    """
    Called when backpressure is released and the connection is writeable again.
    """
    None

  fun ref on_closed(conn: ServerTCPConnection ref) =>
    """
    Called when the connection is closed.
    """
    None

  fun ref on_start_failure(conn: ServerTCPConnection ref,
    reason: net.StartFailureReason)
    """
    Called when a server connection fails to start — for example, an SSL
    handshake failure before `on_accepted` would have fired.
    """

  fun ref on_tls_ready(conn: ServerTCPConnection ref) =>
    """
    Called when a TLS handshake initiated by `start_tls()` completes. The
    connection is now encrypted.
    """
    None

  fun ref on_tls_failure(conn: ServerTCPConnection ref,
    reason: net.TLSFailureReason)
  =>
    """
    Called when a TLS handshake initiated by `start_tls()` fails. `on_closed`
    follows.
    """
    None

  fun ref on_idle_timeout(conn: ServerTCPConnection ref) =>
    """
    Called when no data has been sent or received for the duration configured
    by `conn.idle_timeout()`. The timer re-arms automatically.
    """
    None

  fun ref on_idle_timer_failure(conn: ServerTCPConnection ref) =>
    """
    Called when the idle timer's ASIO event subscription fails. The timer has
    been cancelled; call `conn.idle_timeout(duration)` to retry.
    """
    None

  fun ref on_timer(conn: ServerTCPConnection ref, token: net.TimerToken) =>
    """
    Called when a one-shot timer created by `conn.set_timer()` fires. The
    token matches the one returned by `set_timer()`. Call `set_timer()` again
    to re-arm.
    """
    None

  fun ref on_timer_failure(conn: ServerTCPConnection ref) =>
    """
    Called when a user timer's ASIO event subscription fails. The timer has
    been cancelled; call `conn.set_timer(duration)` to create a new one.
    """
    None
