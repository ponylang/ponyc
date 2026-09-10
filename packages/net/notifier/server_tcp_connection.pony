use net = ".."

actor ServerTCPConnection is
  (net.TCPConnectionActor & net.ServerLifecycleEventReceiver)
  """
  A server-side TCP connection actor driven by a
  [`ServerTCPConnectionNotify`](/net/notifier-ServerTCPConnectionNotify/).

  Created by [`TCPListener`](/net/notifier-TCPListener/) when a client
  connects — there is no public constructor. Wraps `TCPConnection` and
  `ServerLifecycleEventReceiver`, forwarding lifecycle
  callbacks to the notifier returned by `TCPListenNotify.on_connected()`.

  ## Sending data

  `write` and `writev` are fire-and-forget behaviors: they call `send()` on
  the underlying connection and discard the result. A send that cannot be
  accepted (connection not open, backpressure) is silently dropped — no
  callback fires for it. An accepted send produces `on_send_accepted`, then
  exactly one of `on_sent` or `on_send_failed` on the notifier.

  ## Synchronous methods

  Callbacks receive `conn: ServerTCPConnection ref`. Synchronous methods on
  the actor — `buffer_until`, socket options, `close`, `set_timer`, etc. —
  are callable from any callback and take effect immediately.
  """
  var _tcp_connection: net.TCPConnection = net.TCPConnection.none()
  var _notify: ServerTCPConnectionNotify ref

  new _create(auth: net.TCPServerAuth,
    fd: U32,
    notify: ServerTCPConnectionNotify iso,
    read_buffer_size: net.ReadBufferSize = net.DefaultReadBufferSize())
  =>
    """
    Create a plaintext server connection from an accepted socket.
    """
    _notify = consume notify
    _tcp_connection =
      net.TCPConnection.server(
        auth, fd, this, this, read_buffer_size)

  new _ssl_create(auth: net.TCPServerAuth,
    ctx: net.SSLContext val,
    fd: U32,
    notify: ServerTCPConnectionNotify iso,
    read_buffer_size: net.ReadBufferSize = net.DefaultReadBufferSize())
  =>
    """
    Create an SSL server connection from an accepted socket.
    """
    _notify = consume notify
    _tcp_connection =
      net.TCPConnection.ssl_server(
        auth, ctx, fd, this, this, read_buffer_size)

  // --- TCPConnectionActor ---------------------------------------------------
  fun ref _connection(): net.TCPConnection =>
    _tcp_connection

  // --- Behaviors -------------------------------------------------------------
  be write(data: ByteSeq) =>
    """
    Send data on this connection. Fire-and-forget: the send is silently
    dropped if the connection is not open or is under backpressure. An
    accepted send produces `on_send_accepted`, then `on_sent` or
    `on_send_failed` on the notifier.
    """
    _tcp_connection.send(data)

  be writev(data: ByteSeqIter) =>
    """
    Send multiple buffers in a single syscall. Same fire-and-forget semantics
    as `write`.
    """
    _tcp_connection.send(data)

  be mute() =>
    """
    Stop reading from the socket until `unmute` is called. Takes effect on
    the next turn, not immediately — use `YieldReading` from `on_received`
    for an immediate one-shot pause.
    """
    _tcp_connection.mute()

  be unmute() =>
    """
    Resume reading after a `mute`.
    """
    _tcp_connection.unmute()

  // --- Synchronous methods ---------------------------------------------------
  fun ref buffer_until(qty: (net.BufferSize | net.Streaming)):
    net.BufferUntilResult
  =>
    """
    Set the number of bytes to buffer before delivering data via `on_received`.
    Pass `Streaming` to deliver all available data as it arrives.
    """
    _tcp_connection.buffer_until(qty)

  fun ref close() =>
    """
    Gracefully close the connection. Sends already accepted by the underlying
    connection are delivered before close completes. Because `close` is
    synchronous and `write` is a behavior, a `write` call in the same
    callback runs after `close` — use `write` before `close`, not after.
    """
    _tcp_connection.close()

  fun ref hard_close() =>
    """
    Close the connection immediately, dropping any queued data.
    """
    _tcp_connection.hard_close()

  fun ref start_tls(ssl_ctx: net.SSLContext val, host: String = ""):
    (None | net.StartTLSError)
  =>
    """
    Initiate a TLS handshake on an established plaintext connection. Returns
    `None` when the handshake starts, or a `StartTLSError` if the upgrade
    cannot proceed. On success, `on_tls_ready` fires. On failure,
    `on_tls_failure` fires followed by `on_closed`.
    """
    _tcp_connection.start_tls(ssl_ctx, host)

  fun set_nodelay(state: Bool): U32 =>
    """
    Turn Nagle on/off. Returns 0 on success, or a non-zero errno on failure.
    """
    _tcp_connection.set_nodelay(state)

  fun ref keepalive(secs: U32) =>
    """
    Set the TCP keepalive timeout. Pass 0 to disable.
    """
    _tcp_connection.keepalive(secs)

  fun ref local_address(): net.NetAddress =>
    """
    Return the local IP address.
    """
    _tcp_connection.local_address()

  fun ref remote_address(): net.NetAddress =>
    """
    Return the remote IP address.
    """
    _tcp_connection.remote_address()

  fun ref idle_timeout(duration: (net.IdleTimeout | None)) =>
    """
    Set or disable the idle timeout. The timer fires when no data is sent or
    received for the configured duration. Pass `None` to disable.
    """
    _tcp_connection.idle_timeout(duration)

  fun ref set_timer(duration: net.TimerDuration):
    (net.TimerToken | net.SetTimerError)
  =>
    """
    Create a one-shot timer. Returns a `TimerToken` on success. Only one
    timer can be active at a time; cancel the existing one first.
    """
    _tcp_connection.set_timer(duration)

  fun ref cancel_timer(token: net.TimerToken) =>
    """
    Cancel an active timer. Safe to call with stale tokens.
    """
    _tcp_connection.cancel_timer(token)

  fun ref set_read_buffer_minimum(new_min: net.ReadBufferSize):
    (net.ReadBufferResized | net.ReadBufferResizeBelowBufferSize)
  =>
    """
    Set the shrink-back floor for the read buffer.
    """
    _tcp_connection.set_read_buffer_minimum(new_min)

  fun ref resize_read_buffer(size': net.ReadBufferSize):
    net.ReadBufferResizeResult
  =>
    """
    Force the read buffer to a specific size.
    """
    _tcp_connection.resize_read_buffer(size')

  fun get_so_rcvbuf(): (U32, U32) =>
    """
    Get the OS receive buffer size. Returns (errno, value).
    """
    _tcp_connection.get_so_rcvbuf()

  fun set_so_rcvbuf(bufsize: U32): U32 =>
    """
    Set the OS receive buffer size. Returns 0 on success, or errno.
    """
    _tcp_connection.set_so_rcvbuf(bufsize)

  fun get_so_sndbuf(): (U32, U32) =>
    """
    Get the OS send buffer size. Returns (errno, value).
    """
    _tcp_connection.get_so_sndbuf()

  fun set_so_sndbuf(bufsize: U32): U32 =>
    """
    Set the OS send buffer size. Returns 0 on success, or errno.
    """
    _tcp_connection.set_so_sndbuf(bufsize)

  fun getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    """
    Get a socket option as a U32. Returns (errno, value).
    """
    _tcp_connection.getsockopt_u32(level, option_name)

  fun setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    """
    Set a socket option as a U32. Returns 0 on success, or errno.
    """
    _tcp_connection.setsockopt_u32(level, option_name, option)

  fun is_closed(): Bool =>
    """
    True when the connection is closed or closing.
    """
    _tcp_connection.is_closed()

  fun is_writeable(): Bool =>
    """
    True when the socket can currently send.
    """
    _tcp_connection.is_writeable()

  // --- ServerLifecycleEventReceiver ------------------------------------------
  fun ref _on_started() =>
    _notify.on_accepted(this)

  fun ref _on_received(data: Array[U8] iso): net.ReadAction =>
    _notify.on_received(this, consume data)

  fun ref _on_send_accepted(token: net.SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    _notify.on_send_accepted(this, token, data)

  fun ref _on_sent(token: net.SendToken) =>
    _notify.on_sent(this, token)

  fun ref _on_send_failed(token: net.SendToken) =>
    _notify.on_send_failed(this, token)

  fun ref _on_throttled() =>
    _notify.on_throttled(this)

  fun ref _on_unthrottled() =>
    _notify.on_unthrottled(this)

  fun ref _on_closed() =>
    _notify.on_closed(this)

  fun ref _on_start_failure(reason: net.StartFailureReason) =>
    _notify.on_start_failure(this, reason)

  fun ref _on_tls_ready() =>
    _notify.on_tls_ready(this)

  fun ref _on_tls_failure(reason: net.TLSFailureReason) =>
    _notify.on_tls_failure(this, reason)

  fun ref _on_idle_timeout() =>
    _notify.on_idle_timeout(this)

  fun ref _on_idle_timer_failure() =>
    _notify.on_idle_timer_failure(this)

  fun ref _on_timer(token: net.TimerToken) =>
    _notify.on_timer(this, token)

  fun ref _on_timer_failure() =>
    _notify.on_timer_failure(this)
