use "collections"

class TCPConnection[TCP: TCPBackend ref = RuntimeBackend]
  """
  The TCP connection: all connection state and I/O, including SSL. A
  `TCPConnectionActor` owns one and delegates to it.

  Create it with one of the four constructors -- `client`, `server`,
  `ssl_client`, `ssl_server` -- using `TCPConnection.none()` as the field
  initializer before that. An open plaintext connection can be upgraded to TLS
  with `start_tls`. See the package documentation for the full lifecycle.
  """
  var _tcp: TCP = TCP
  var _state: _ConnectionState[TCP] ref = _ConnectionNone[TCP]
  var _shutdown: Bool = false
  var _throttled: Bool = false
  var _readable: Bool = false
  var _writeable: Bool = false
  var _muted: Bool = false
  // Happy Eyeballs
  var _inflight_events: Array[AsioEventID] = Array[AsioEventID]
  var _fd: U32 = -1
  var _event: AsioEventID = AsioEvent.none()
  var _spawned_by: (TCPListenerActor[TCP] | None) = None
  let _lifecycle_event_receiver:
    (ClientLifecycleEventReceiver[TCP] ref
    | ServerLifecycleEventReceiver[TCP] ref
    | None)
  let _enclosing: (TCPConnectionActor[TCP] ref | None)
  embed _pending: _PendingWrites = _PendingWrites
  var _read_buffer: Array[U8] iso = recover Array[U8] end
  var _bytes_in_read_buffer: USize = 0
  var _read_buffer_size: USize = 16384
  var _read_buffer_min: USize = 16384
  var _buffer_until: (BufferSize | Streaming) = Streaming
  // Send token tracking. _pending_tokens is a FIFO of (completion offset,
  // token): each accepted send records the cumulative byte offset -- into the
  // wire-byte stream `_pending` drains (ciphertext for SSL, plaintext
  // otherwise) -- at which its bytes finish. _on_sent fires for a token once
  // _cumulative_sent reaches its offset. Both counters reset to 0 whenever the
  // queue empties, so the offsets stay small.
  var _next_token_id: USize = 0
  embed _pending_tokens: List[(USize, SendToken)] = _pending_tokens.create()
  var _cumulative_enqueued: USize = 0
  var _cumulative_sent: USize = 0
  // Built-in SSL support
  var _ssl: _TLSState = _NoTLS
  var _close_notify_pending: Bool = false
  // Set when connect returned a non-empty array, meaning at least one TCP
  // connection attempt was made. Used by the failure callback to distinguish
  // DNS failure (no attempts) from TCP failure (all attempts failed).
  var _had_inflight: Bool = false
  // Per-connection idle timeout via ASIO timer
  var _timer_event: AsioEventID = AsioEvent.none()
  var _idle_timeout_nsec: U64 = 0
  // Per-connection connect timeout via ASIO timer (one-shot)
  var _connect_timer_event: AsioEventID = AsioEvent.none()
  var _connect_timeout_nsec: U64 = 0
  // Per-connection user timer via ASIO timer (one-shot, no I/O reset)
  var _user_timer_event: AsioEventID = AsioEvent.none()
  var _next_timer_id: USize = 0
  var _user_timer_token: (TimerToken | None) = None
  // client startup state
  var _host: String = ""
  var _port: String = ""
  var _from: String = ""
  var _ip_version: IPVersion = DualStack

  fun ref _tcp_ops(): TCP =>
    """
    The TCP operations backend for this connection, used by state classes
    that need to call receive.
    """
    _tcp

  new client(auth: TCPConnectAuth,
    host: String,
    port: String,
    from: String,
    enclosing: TCPConnectionActor[TCP] ref,
    ler: ClientLifecycleEventReceiver[TCP] ref,
    read_buffer_size: ReadBufferSize = DefaultReadBufferSize(),
    ip_version: IPVersion = DualStack,
    connection_timeout: (ConnectionTimeout | None) = None)
  =>
    """
    Create a client-side plaintext connection. An optional `connection_timeout`
    bounds the TCP Happy Eyeballs phase. If the timeout fires before
    `_on_connected`, the connection fails with `ConnectionFailedTimeout`.
    """
    _lifecycle_event_receiver = ler
    _enclosing = enclosing
    _host = host
    _port = port
    _from = from
    _read_buffer_size = read_buffer_size()
    _read_buffer_min = read_buffer_size()
    _ip_version = ip_version
    match connection_timeout
    | let ct: ConnectionTimeout => _connect_timeout_nsec = ct() * 1_000_000
    end

    _resize_read_buffer_if_needed()

    enclosing._finish_initialization()

  new server(auth: TCPServerAuth,
    fd': U32,
    enclosing: TCPConnectionActor[TCP] ref,
    ler: ServerLifecycleEventReceiver[TCP] ref,
    read_buffer_size: ReadBufferSize = DefaultReadBufferSize())
  =>
    """
    Create a server-side plaintext connection from an accepted socket `fd'`.
    """
    _fd = fd'
    _lifecycle_event_receiver = ler
    _enclosing = enclosing
    _read_buffer_size = read_buffer_size()
    _read_buffer_min = read_buffer_size()

    _resize_read_buffer_if_needed()

    enclosing._finish_initialization()

  new ssl_client(auth: TCPConnectAuth,
    ssl_ctx: SSLContext val,
    host: String,
    port: String,
    from: String,
    enclosing: TCPConnectionActor[TCP] ref,
    ler: ClientLifecycleEventReceiver[TCP] ref,
    read_buffer_size: ReadBufferSize = DefaultReadBufferSize(),
    ip_version: IPVersion = DualStack,
    connection_timeout: (ConnectionTimeout | None) = None)
  =>
    """
    Create a client-side SSL connection. The SSL session is created from the
    provided SSLContext. If session creation fails, the connection reports
    failure asynchronously via _on_connection_failure(ConnectionFailedSSL).
    An optional `connection_timeout` bounds the connect-to-ready phase
    (TCP Happy Eyeballs + TLS handshake). If the timeout fires before
    `_on_connected`, the connection fails with `ConnectionFailedTimeout`.
    """
    _lifecycle_event_receiver = ler
    _enclosing = enclosing
    _host = host
    _port = port
    _from = from
    _read_buffer_size = read_buffer_size()
    _read_buffer_min = read_buffer_size()
    _ip_version = ip_version
    match connection_timeout
    | let ct: ConnectionTimeout => _connect_timeout_nsec = ct() * 1_000_000
    end

    _ssl = _MakeTLS.client(ssl_ctx, host)

    _resize_read_buffer_if_needed()

    enclosing._finish_initialization()

  new ssl_server(auth: TCPServerAuth,
    ssl_ctx: SSLContext val,
    fd': U32,
    enclosing: TCPConnectionActor[TCP] ref,
    ler: ServerLifecycleEventReceiver[TCP] ref,
    read_buffer_size: ReadBufferSize = DefaultReadBufferSize())
  =>
    """
    Create a server-side SSL connection. The SSL session is created from the
    provided SSLContext. If session creation fails, the connection reports
    failure asynchronously via _on_start_failure(StartFailedSSL) and closes the
    fd.
    """
    _fd = fd'
    _lifecycle_event_receiver = ler
    _enclosing = enclosing
    _read_buffer_size = read_buffer_size()
    _read_buffer_min = read_buffer_size()

    _ssl = _MakeTLS.server(ssl_ctx)

    _resize_read_buffer_if_needed()

    enclosing._finish_initialization()

  new none() =>
    _enclosing = None
    _lifecycle_event_receiver = None

  fun ref keepalive(secs: U32) =>
    """
    Sets the TCP keepalive timeout to approximately `secs` seconds. Exact
    timing is OS dependent. If `secs` is zero, TCP keepalive is disabled. TCP
    keepalive is disabled by default. This can only be set on a connected
    socket.
    """
    _state.keepalive(this, secs)

  fun set_nodelay(state: Bool): U32 =>
    """
    Turn Nagle on/off. Defaults to on (Nagle enabled, nodelay off). When
    enabled (`state = true`), small writes are sent immediately without
    waiting to coalesce — useful for latency-sensitive protocols. When
    disabled (`state = false`), the OS may buffer small writes.

    Returns 0 on success, or a non-zero errno on failure. Only meaningful
    on a connected socket — returns non-zero if the connection is not open.
    """
    setsockopt_u32(
      OSSockOpt.ipproto_tcp(),
      OSSockOpt.tcp_nodelay(),
      if state then 1 else 0 end)

  fun get_so_rcvbuf(): (U32, U32) =>
    """
    Get the OS receive buffer size for this socket.

    Returns a 2-tuple: (errno, value). On success, errno is 0 and value is
    the buffer size in bytes. On failure, errno is non-zero and value should
    be ignored. Only meaningful on a connected socket — returns (1, 0) if
    the connection is not open.
    """
    getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())

  fun set_so_rcvbuf(bufsize: U32): U32 =>
    """
    Set the OS receive buffer size for this socket. The OS may round the
    requested size up to a minimum or clamp it to a maximum.

    Returns 0 on success, or a non-zero errno on failure. Only meaningful
    on a connected socket — returns non-zero if the connection is not open.
    """
    setsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), bufsize)

  fun get_so_sndbuf(): (U32, U32) =>
    """
    Get the OS send buffer size for this socket.

    Returns a 2-tuple: (errno, value). On success, errno is 0 and value is
    the buffer size in bytes. On failure, errno is non-zero and value should
    be ignored. Only meaningful on a connected socket — returns (1, 0) if
    the connection is not open.
    """
    getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())

  fun set_so_sndbuf(bufsize: U32): U32 =>
    """
    Set the OS send buffer size for this socket. The OS may round the
    requested size up to a minimum or clamp it to a maximum.

    Returns 0 on success, or a non-zero errno on failure. Only meaningful
    on a connected socket — returns non-zero if the connection is not open.
    """
    setsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf(), bufsize)

  fun getsockopt(level: I32,
    option_name: I32,
    option_max_size: USize = 4)
    : (U32, Array[U8] iso^)
  =>
    """
    General interface to `getsockopt(2)` for accessing any socket option.

    The `option_max_size` argument is the maximum number of bytes the caller
    expects the kernel to return. This method allocates a buffer of that size
    before calling `getsockopt(2)`.

    Returns a 2-tuple: on success, `(0, data)` where `data` is the bytes
    returned by the kernel, sized to the actual length the kernel wrote. On
    failure, `(errno, undefined)` — the second element must be ignored. Only
    meaningful on a connected socket — returns `(1, empty)` if the connection
    is not open.

    For commonly-tuned options, prefer the dedicated convenience methods
    (`set_nodelay`, `get_so_rcvbuf`, etc.). Do not change the socket's
    non-blocking mode — the net package's event-driven I/O requires non-blocking
    sockets.
    """
    _state.getsockopt(this, level, option_name, option_max_size)

  fun getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    """
    Wrapper for `getsockopt(2)` where the kernel returns a C `uint32_t`.

    Returns a 2-tuple: on success, `(0, value)`. On failure,
    `(errno, undefined)` — the second element must be ignored. Only
    meaningful on a connected socket — returns `(1, 0)` if the connection
    is not open.

    For commonly-tuned options, prefer the dedicated convenience methods
    (`get_so_rcvbuf`, `get_so_sndbuf`, etc.). Do not change the socket's
    non-blocking mode — the net package's event-driven I/O requires non-blocking
    sockets.
    """
    _state.getsockopt_u32(this, level, option_name)

  fun setsockopt(level: I32, option_name: I32, option: Array[U8]): U32 =>
    """
    General interface to `setsockopt(2)` for setting any socket option.

    The caller is responsible for the correct size, byte contents, and
    byte order of the `option` array for the requested `level` and
    `option_name`.

    Returns 0 on success, or the value of `errno` on failure. Only
    meaningful on a connected socket — returns non-zero if the connection
    is not open.

    For commonly-tuned options, prefer the dedicated convenience methods
    (`set_nodelay`, `set_so_rcvbuf`, etc.). Do not change the socket's
    non-blocking mode — the net package's event-driven I/O requires non-blocking
    sockets.
    """
    _state.setsockopt(this, level, option_name, option)

  fun setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    """
    Wrapper for `setsockopt(2)` where the kernel expects a C `uint32_t`.

    Returns 0 on success, or the value of `errno` on failure. Only
    meaningful on a connected socket — returns non-zero if the connection
    is not open.

    For commonly-tuned options, prefer the dedicated convenience methods
    (`set_nodelay`, `set_so_rcvbuf`, etc.). Do not change the socket's
    non-blocking mode — the net package's event-driven I/O requires non-blocking
    sockets.
    """
    _state.setsockopt_u32(this, level, option_name, option)

  fun ref idle_timeout(duration: (IdleTimeout | None)) =>
    """
    Set or disable the idle timeout. Idle timeout is disabled by default.

    When `duration` is an `IdleTimeout`, the timer fires when no successful
    send or receive occurs for that duration, delivering
    `_on_idle_timeout()` to the lifecycle event receiver. When `duration`
    is `None`, the idle timeout is disabled.

    The timer re-arms after each firing while the connection is open.
    Both `hard_close()` and `close()` cancel it.

    Can be called before the connection is established — the value is
    stored and the timer starts when the connection is ready.

    This is independent of TCP keepalive (`keepalive()`). TCP keepalive
    is a transport-level probe that detects dead peers. Idle timeout is
    application-level inactivity detection — it fires whether or not the
    peer is alive.

    If the idle timer's ASIO event subscription fails asynchronously
    (e.g. `ENOMEM` from `kevent`/`epoll_ctl`), the timer is cancelled and
    `_on_idle_timer_failure()` is dispatched to the lifecycle event
    receiver.
    """
    _state.idle_timeout(this, duration)

  fun ref set_timer(duration: TimerDuration): (TimerToken | SetTimerError) =>
    """
    Create a one-shot timer that fires `_on_timer()` after the configured
    duration. Returns a `TimerToken` on success, or a `SetTimerError` on
    failure.

    Unlike `idle_timeout()`, this timer has no I/O-reset behavior — it fires
    unconditionally after the duration elapses, regardless of send/receive
    activity. There is no automatic re-arming; call `set_timer()` again from
    `_on_timer()` for repetition.

    Only one user timer can be active at a time. Setting a timer while one is
    already active returns `SetTimerAlreadyActive` — call `cancel_timer()`
    first. This prevents silent token invalidation.

    Requires the connection to be application-level connected: the connection
    must be open and the initial SSL handshake (if any) must have completed.
    TLS upgrades via `start_tls()` do not block timer creation.

    The timer survives `close()` (graceful shutdown) but is cancelled by
    `hard_close()`.

    User timers have two error paths. This method returns a
    `SetTimerError` synchronously when preconditions prevent the timer
    from being created (see the return type). When creation succeeds but
    the ASIO event subscription later fails (e.g. `ENOMEM` from
    `kevent`/`epoll_ctl`), `_on_timer_failure()` is dispatched to the
    lifecycle event receiver.
    """
    _state.set_timer(this, duration)

  fun ref cancel_timer(token: TimerToken) =>
    """
    Cancel an active timer. No-op if the token doesn't match the active timer
    (already fired, already cancelled, wrong token). Safe to call with stale
    tokens.

    No connection state check — timers can be cancelled during graceful
    shutdown (`_Closing`) since they remain active until `hard_close()`.
    """
    match _user_timer_token
    | let t: TimerToken if t == token =>
      PonyAsio.unsubscribe(_user_timer_event)
      _user_timer_event = AsioEvent.none()
      _user_timer_token = None
    end

  fun ref set_read_buffer_minimum(new_min: ReadBufferSize):
    (ReadBufferResized | ReadBufferResizeBelowBufferSize)
  =>
    """
    Set the shrink-back floor for the read buffer to exactly `new_min` bytes.
    When the read buffer is empty and larger than the minimum, it shrinks back
    to this size automatically. If the current buffer allocation is smaller
    than `new_min`, the buffer is grown to match.

    Returns `ReadBufferResizeBelowBufferSize` if `new_min` is less than the
    current buffer-until value.
    """
    let min = new_min()

    if min < _user_buffer_until() then
      return ReadBufferResizeBelowBufferSize
    end

    _read_buffer_min = min

    if _read_buffer_size < min then
      _read_buffer_size = min
      _read_buffer.undefined(_read_buffer_size)
    end

    ReadBufferResized

  fun ref resize_read_buffer(size': ReadBufferSize): ReadBufferResizeResult =>
    """
    Force the read buffer to exactly `size'` bytes, reallocating if different.
    If `size'` is below the current minimum, the minimum is lowered to match.

    Returns `ReadBufferResizeBelowBufferSize` if `size'` is less than the
    current buffer-until value, or `ReadBufferResizeBelowUsed` if `size'` is
    less than the amount of unprocessed data currently in the buffer.
    """
    let size = size'()

    if size < _user_buffer_until() then
      return ReadBufferResizeBelowBufferSize
    end

    if size < _bytes_in_read_buffer then
      return ReadBufferResizeBelowUsed
    end

    if size < _read_buffer_min then
      _read_buffer_min = size
    end

    _read_buffer_size = size

    let old_buffer = _read_buffer = recover Array[U8] end
    _read_buffer =
      recover iso
        let a = Array[U8](size)
        a.undefined(size)
        if _bytes_in_read_buffer > 0 then
          (consume old_buffer).copy_to(a, 0, 0, _bytes_in_read_buffer)
        end
        a
      end

    ReadBufferResized

  fun ref local_address(): NetAddress =>
    """
    Return the local IP address. If this TCPConnection is closed then the
    address returned is invalid.
    """
    let ip = recover NetAddress end
    _tcp.sockname(_fd, ip)
    ip

  fun ref remote_address(): NetAddress =>
    """
    Return the remote IP address. If this TCPConnection is closed then the
    address returned is invalid.
    """
    let ip = recover NetAddress end
    _tcp.peername(_fd, ip)
    ip

  fun ref mute() =>
    """
    Temporarily suspend reading off this TCPConnection until such time as
    `unmute` is called.

    When called from `_on_received`, no further data is delivered. Whatever the
    connection has read but not yet delivered is held, and `unmute` delivers it
    before anything read off the socket afterward. This holds for plaintext and
    SSL connections alike.

    Held data only survives to an `unmute`. Closing a muted connection drops it,
    because `close` on a muted connection hard closes and `dispose` always does.
    """
    _muted = true

  fun ref unmute() =>
    """
    Start reading off this TCPConnection again after having been muted.

    Reading resumes on a later turn, not during this call. Data held since the
    `mute` is delivered before anything read off the socket afterward.
    """
    _muted = false
    _set_readable()
    _queue_read()

  fun _user_buffer_until(): USize =>
    """
    The user's requested buffer-until value, regardless of whether SSL is
    active. Returns 0 when `Streaming`, since 0 < any valid buffer min — the
    correct behavior for invariant checks when no buffer-until constraint is
    active.
    """
    match \exhaustive\ _buffer_until
    | let e: BufferSize => e()
    | Streaming => 0
    end

  fun ref buffer_until(qty: (BufferSize | Streaming)): BufferUntilResult =>
    """
    Set the number of bytes to buffer before delivering data via
    `_on_received`. When `qty` is `Streaming`, all available data is delivered
    as it arrives.

    Returns `BufferSizeAboveMinimum` if `qty` exceeds the current read
    buffer minimum. Raise the buffer minimum first, then set buffer_until.
    """
    match qty
    | let e: BufferSize =>
      if e() > _read_buffer_min then
        return BufferSizeAboveMinimum
      end
    end

    match \exhaustive\ _lifecycle_event_receiver
    | let _: EitherLifecycleEventReceiver[TCP] =>
      _buffer_until = qty
    | None =>
      _Unreachable()
    end

    BufferUntilSet

  fun ref close() =>
    """
    Gracefully close the connection. Data already handed to an accepted
    `send()` is delivered before the connection closes.

    On a muted connection this is a hard close instead: it shuts down at once
    and drops undelivered data — both held reads and queued writes (the writes
    fail with `_on_send_failed`).

    Closing before the connection is established abandons the attempt and
    delivers `_on_connection_failure`.
    """
    if _muted then
      hard_close()
    else
      _state.close(this)
    end

  fun ref hard_close() =>
    """
    When an error happens, do a non-graceful close.
    """
    _hard_close(_UnspecifiedCause)

  fun ref _hard_close(cause: _HardCloseCause) =>
    """
    Hard close, saying why. The caller that knows the cause passes it; the
    state decides which failure callback it becomes. `_UnspecifiedCause` where
    there is nothing to add and the state's default reason applies.
    """
    _state.hard_close(this, cause)

  fun ref _hard_close_connecting(cause: _HardCloseCause) =>
    """
    Hard close during the connecting phase. Disposes SSL, fires the
    appropriate failure callback, cancels all timers, and unsubscribes
    any pending Happy Eyeballs socket events so the runtime can exit
    without waiting for OS-level connect timeouts.
    """
    _state = _Closed[TCP]
    _cancel_inflight_events()
    _dispose_tls()
    match _lifecycle_event_receiver
    | let c: ClientLifecycleEventReceiver[TCP] ref =>
      // `_had_inflight` is state, not a cause: it records whether any TCP
      // attempt ever started, which is what separates a DNS failure from a
      // TCP one. No caller knows it.
      let reason =
        match cause
        | _ConnectTimerFailed => ConnectionFailedTimerError
        | _ConnectTimedOut => ConnectionFailedTimeout
        else
          if _had_inflight then
            ConnectionFailedTCP
          else
            ConnectionFailedDNS
          end
        end
      c._on_connection_failure(reason)
    end
    _cancel_idle_timer()
    _cancel_connect_timer()
    _cancel_user_timer()

  fun ref _hard_close_cleanup() =>
    """
    Common teardown for hard-closing an established connection. Cancels any
    remaining Happy Eyeballs straggler events, gives every pending token its
    terminal callback, clears pending buffers, cancels all timers,
    unsubscribes the event, releases the fd (see `_close_event_fd` — closed
    here on POSIX, deferred to the unsubscribe REMOVE on Windows), and
    disposes SSL. Order is load-bearing: inflight cancel before own-event
    unsubscribe, timer cancel before event unsubscribe, SSL dispose after the
    fd is released.

    Runs with `_state` already `_Closed`: the `_hard_close_*` methods set it
    before calling this, so the callbacks this fires (and any re-entrant call
    the application makes from them) see a closed connection.
    """
    _cancel_inflight_events()
    // Split the queue on the same completion test `_fire_completed_sends`
    // uses. A hard close can land partway through that method's reporting
    // loop -- it fires `_on_sent` for one token, and the application closes
    // from it while later tokens whose bytes went out in the same write are
    // still queued. Those reached the OS, so they are sent, not failed.
    // `_on_sent` stays a direct call so it still precedes `_on_closed`, which
    // the `_hard_close_*` methods fire after this returns.
    match _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      try
        while _pending_tokens.size() > 0 do
          (let offset, let token) = _pending_tokens.shift()?
          if offset <= _cumulative_sent then
            _fire_on_sent(token)
          else
            e._notify_send_failed(token)
          end
        end
      else
        // Guarded by size() > 0, so shift() never errors.
        _Unreachable()
      end
    end

    _pending.clear()
    _cumulative_enqueued = 0
    _cumulative_sent = 0

    _cancel_idle_timer()
    _cancel_connect_timer()
    _cancel_user_timer()
    PonyAsio.unsubscribe(_event)
    _set_unreadable()
    _set_unwriteable()
    _bytes_in_read_buffer = 0
    _throttled = false
    _close_notify_pending = false

    _close_event_fd(_fd)
    _fd = -1

    _dispose_tls()

  fun ref _mark_close_notify_pending() =>
    match _ssl
    | let _: _TLS => _close_notify_pending = true
    end

  fun ref _close_notify_then_shutdown() =>
    """
    Send TLS `close_notify` (if applicable) and then TCP FIN. Called from
    `_Closing.drained()` when the application's write queue is empty.

    `SSL_shutdown` must happen here, not earlier: it makes `SSL_read` return
    `SSL_ERROR_ZERO_RETURN`, so calling it during `close()` would discard
    buffered TLS records the read loop has not yet delivered.

    This runs from inside `_send_pending_writes()` (via `drained()`), so it
    must not call `_send_pending_writes()` or `_ssl_flush_sends()` — that
    would re-enter the write loop. Instead it pushes the close_notify
    ciphertext directly onto the pending queue — bypassing `_enqueue()`,
    whose `is_closed()` guard would drop it since `_Closing` reports as
    closed.
    """
    if _close_notify_pending then
      _close_notify_pending = false
      match _ssl
      | let tls: _TLS =>
        tls.session.close()
        // Drain the close_notify ciphertext from the SSL BIO directly into
        // the pending queue. `_enqueue()` cannot be used here: the connection
        // is in `_Closing`, where `is_closed() = true`, so `_enqueue()`
        // silently drops the data.
        while true do
          match \exhaustive\ tls.session.send()
          | let data: Array[U8] iso =>
            let s = data.size()
            _pending.push(consume data)
            _cumulative_enqueued = _cumulative_enqueued + s
          | None => break
          end
        end
        if _has_pending_writes() then
          _set_unwriteable()
          PonyAsio.resubscribe_write(_event)
          return
        end
      end
    end

    _initiate_shutdown()

  fun ref _dispose_tls() =>
    """
    Dispose the SSL session and record that it is gone. The only place that
    disposes one: `_TLS` means the session is alive, and nothing else may take
    that away.
    """
    match _ssl
    | let tls: _TLS =>
      tls.session.dispose()
      _ssl = _TLSDisposed
    end

  fun ref _spawner_notification() =>
    """
    Notify the spawning listener (if any) that this server connection has
    closed. For client connections, this is a no-op.
    """
    match _lifecycle_event_receiver
    | let e: ServerLifecycleEventReceiver[TCP] ref =>
      match \exhaustive\ _spawned_by
      | let spawner: TCPListenerActor[TCP] =>
        spawner._connection_closed()
        _spawned_by = None
      | None =>
        // It is possible that we didn't yet receive the message giving us
        // our spawner. Do nothing in that case.
        None
      end
    end

  fun ref _hard_close_connected() =>
    """
    Hard close for an established connection where the application has been
    notified (i.e., _on_connected/_on_started has already fired). Only
    reachable from `_Open` and `_Closing` — handshake states have their own
    hard-close methods. Fires `_on_closed` and notifies the spawner.
    """
    _state = _Closed[TCP]
    _hard_close_cleanup()

    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_closed()
    | None =>
      _Unreachable()
    end

    _spawner_notification()

  fun ref _hard_close_ssl_handshaking(cause: _HardCloseCause) =>
    """
    Hard close during the initial SSL handshake (state: `_SSLHandshaking`).
    The application has not been notified — fires `_on_connection_failure`
    (client) or `_on_start_failure` (server).
    """
    _state = _Closed[TCP]
    _hard_close_cleanup()

    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      match \exhaustive\ s
      | let c: ClientLifecycleEventReceiver[TCP] ref =>
        let reason =
          match cause
          | _ConnectTimerFailed => ConnectionFailedTimerError
          | _ConnectTimedOut => ConnectionFailedTimeout
          else ConnectionFailedSSL
          end
        c._on_connection_failure(reason)
      | let srv: ServerLifecycleEventReceiver[TCP] ref =>
        srv._on_start_failure(StartFailedSSL)
      end
    | None =>
      _Unreachable()
    end

    _spawner_notification()

  fun ref _hard_close_tls_upgrading(cause: _HardCloseCause) =>
    """
    Hard close during a TLS upgrade handshake (state: `_TLSUpgrading`).
    The application was already notified of the plaintext connection, so
    `_on_tls_failure` fires followed by `_on_closed`.
    """
    _state = _Closed[TCP]
    _hard_close_cleanup()

    let reason =
      match cause
      | _TLSAuthFailure => TLSAuthFailed
      else TLSGeneralError
      end

    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_tls_failure(reason)
      s._on_closed()
    | None =>
      _Unreachable()
    end

    _spawner_notification()

  fun is_closed(): Bool =>
    """
    Returns whether the connection is closed or closing.
    """
    _state.is_closed()

  fun is_writeable(): Bool =>
    """
    Returns whether the socket can currently send.
    """
    _state.sends_allowed() and _writeable

  fun ref start_tls(ssl_ctx: SSLContext val, host: String = ""):
    (None | StartTLSError)
  =>
    """
    Initiate a TLS handshake on an established plaintext connection. Returns
    `None` when the handshake has been started, or a `StartTLSError` if the
    upgrade cannot proceed (the connection is unchanged in that case).

    Preconditions: the connection must be open, not already TLS, not muted,
    have no unprocessed data in the read buffer, and have no pending writes.
    The read buffer check prevents a man-in-the-middle from injecting pre-TLS
    data that the application would process as post-TLS (CVE-2021-23222).

    On success, `_on_tls_ready()` fires when the handshake completes. During
    the handshake, `send()` returns `SendErrorNotConnected`. If the handshake
    fails, `_on_tls_failure` fires followed by `_on_closed()`.

    The `host` parameter is used for SNI (Server Name Indication) on client
    connections. Pass an empty string for server connections or when SNI is
    not needed.
    """
    _state.start_tls(this, ssl_ctx, host)

  fun ref _do_keepalive(secs: U32) =>
    _tcp.keepalive(_fd, secs)

  fun _do_getsockopt(level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    _OSSocket.getsockopt(_fd, level, option_name, option_max_size)

  fun _do_getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    _OSSocket.getsockopt_u32(_fd, level, option_name)

  fun _do_setsockopt(level: I32, option_name: I32, option: Array[U8]): U32 =>
    _OSSocket.setsockopt(_fd, level, option_name, option)

  fun _do_setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    _OSSocket.setsockopt_u32(_fd, level, option_name, option)

  fun ref _do_start_tls(ssl_ctx: SSLContext val, host: String):
    (None | StartTLSError)
  =>
    match _ssl
    | _NoTLS => None
    else
      return StartTLSAlreadyTLS
    end

    // sendv is synchronous on every platform now — it returns OK with a
    // byte count, Retry on EWOULDBLOCK, or Error. Any remaining pending
    // bytes mean the write didn't fully drain, so the TLS upgrade must wait.
    if _muted or (_bytes_in_read_buffer > 0) or _has_pending_writes() then
      return StartTLSNotReady
    end

    let ssl =
      try
        match \exhaustive\ _lifecycle_event_receiver
        | let _: ClientLifecycleEventReceiver[TCP] ref =>
          ssl_ctx.client(host)?
        | let _: ServerLifecycleEventReceiver[TCP] ref =>
          ssl_ctx.server()?
        | None =>
          _Unreachable()
          return StartTLSSessionFailed
        end
      else
        return StartTLSSessionFailed
      end

    _ssl = _TLS(consume ssl)
    _state = _TLSUpgrading[TCP]
    _ssl_flush_sends()
    None

  fun ref send(data: (ByteSeq | ByteSeqIter)): SendResult =>
    """
    Send data on this connection. Accepts a single buffer (`ByteSeq`) or
    multiple buffers (`ByteSeqIter`). When multiple buffers are provided,
    they are sent in a single syscall — avoiding both per-buffer
    syscall overhead and the cost of copying into a contiguous buffer.

    Returns `SendAccepted` on success, or a `SendError` explaining the
    failure. On success `_on_send_accepted(token, data)` has already fired,
    from inside this call and before the bytes were written. That token gets
    exactly one further callback: `_on_sent(token)` once the data has been
    handed to the OS (written to the kernel send buffer, not received by the
    peer), or `_on_send_failed(token)` if the connection is lost or
    hard-closed before the bytes are written. A graceful `close()` sends
    what's still queued, so those sends fire `_on_sent`, not
    `_on_send_failed`. Closing the connection from any callback that runs
    inside this call does not change the return: the send stays accepted.

    Both callbacks can run before this returns, so anything the calling code
    updates after the call -- a counter, a map, a flag -- is not updated yet
    when they fire.
    """
    _state.send(this, data)

  fun ref _do_send(data: (ByteSeq | ByteSeqIter)): SendResult =>
    // Only reachable from _Open.send() — the handshake states return
    // SendErrorNotConnected directly without calling this method.
    if not _writeable then
      return SendErrorNotWriteable
    end

    // Enqueue this send's wire bytes (ciphertext for SSL, plaintext otherwise).
    // For SSL, ssl.write encrypts the whole plaintext synchronously, so all of
    // this send's ciphertext is enqueued here; on an ssl.write error the send
    // failed, so tell the caller and do nothing else.
    match \exhaustive\ _ssl
    | let tls: _TLS =>
      match \exhaustive\ data
      | let d: ByteSeq =>
        try tls.session.write(d)? else return SendErrorNotWriteable end
      | let d: ByteSeqIter =>
        for v in d.values() do
          try tls.session.write(v)? else return SendErrorNotWriteable end
        end
      end
      _ssl_enqueue_sends()
    | _TLSDisposed | _TLSFailed =>
      // `sends_allowed()` is false without a live session.
      _Unreachable()
      return SendErrorNotConnected
    | _NoTLS =>
      match \exhaustive\ data
      | let d: ByteSeq =>
        _enqueue(d)
      | let d: ByteSeqIter =>
        for v in d.values() do
          _enqueue(v)
        end
      end
    end

    // Queue the token before the flush. The flush can end the connection, and a
    // token already on `_pending_tokens` gets its callback either way.
    _next_token_id = _next_token_id + 1
    let token = SendToken._create(_next_token_id)
    _pending_tokens.push((_cumulative_enqueued, token))

    // Hand the token over before the flush: the flush fires `_on_sent`, and
    // the application has to have the token before the callback carrying it.
    _fire_on_send_accepted(token, data)

    _send_pending_writes()

    SendAccepted

  fun ref _initiate_shutdown() =>
    """
    Send FIN to the peer, but only once there is nothing left to send ahead of
    it: no inflight connection attempts, no queued writes, and no pending TLS
    close_notify. Idempotent — sends FIN at most once. `hard_close()` is the
    non-graceful path and still drops queued writes.

    The TLS guard blocks FIN until close_notify has been sent.
    `_close_notify_then_shutdown()` sends the alert when the write queue
    drains, then calls back into this method for FIN.
    """
    if _shutdown or
      _has_inflight_events() or
      _has_pending_writes()
    then
      return
    end

    if _close_notify_pending then
      _set_unwriteable()
      PonyAsio.resubscribe_write(_event)
      return
    end

    _shutdown = true
    _tcp.shutdown(_fd)

  fun ref _enqueue(data: ByteSeq) =>
    """
    Add a buffer to the pending write queue, without flushing. Enqueue and
    flush are separate steps because `_do_send` records a send's completion
    offset between them; `_send_pending_writes()` is the flush.

    `_ssl_enqueue_sends()` calls this during `_SSLHandshaking` to push
    handshake protocol data, so the guard is `is_closed()` — not
    `sends_allowed()`, which is false during the handshake.
    """
    if data.size() == 0 then return end
    if not is_closed() then
      _pending.push(data)
      _cumulative_enqueued = _cumulative_enqueued + data.size()
    end

  fun ref _manage_pending_buffer(bytes_sent: USize) =>
    """
    Account for `bytes_sent` sent from the head of the pending queue. `_pending`
    trims its buffers and advances its offset; `_cumulative_sent` tracks the
    same bytes for token completion.
    """
    _cumulative_sent = _cumulative_sent + bytes_sent
    _pending.sent(bytes_sent)

  fun ref _send_pending_writes() =>
    """
    Flush pending write data using sendv. Synchronous and non-blocking on
    every platform: a partial write or `SocketResultRetry` (the kernel send
    buffer is full) applies backpressure and leaves the rest queued for the
    next writeable event.

    Runs application code: `_on_sent` for each send it completes, and
    `_on_throttled` when it applies backpressure. Either can close the
    connection under the caller.
    """
    let writev_batch_size: USize = _tcp.writev_max().usize()
    var wrote_bytes: Bool = false

    while _writeable and (_pending.total() > 0) do
      try
        // Determine batch size and byte count
        let num_to_send: USize =
          _pending.size().min(writev_batch_size)
        let bytes_to_send: USize = _pending.prefix_total(num_to_send)

        // sendv — three-state result with bytes-sent count
        match \exhaustive\ _tcp.sendv(
          _event, _pending.buffers(), 0, num_to_send, _pending.first_offset())?
        | (SocketResultOk, let len: USize) =>
          if len > 0 then
            wrote_bytes = true
          end
          if len < bytes_to_send then
            _manage_pending_buffer(len)
            _apply_backpressure()
          else
            _manage_pending_buffer(bytes_to_send)
          end
        | (SocketResultRetry, _) =>
          _apply_backpressure()
        | (SocketResultError, _) => error
        end
      else
        // sendv error or unreachable Array.apply bounds — non-graceful
        // shutdown. Fire _on_sent for sends whose bytes already reached the
        // OS in an earlier batch of this flush first, so hard_close fails only
        // the rest.
        _fire_completed_sends()
        hard_close()
        return
      end
    end

    // A drain is outgoing traffic too; reset the idle timer.
    if wrote_bytes then
      _reset_idle_timer()
    end

    _fire_completed_sends()

    if _pending.total() == 0 then
      _release_backpressure()
      _state.drained(this)
    end

  fun ref _fire_completed_sends() =>
    """
    Fire `_on_sent` for each pending send whose bytes have all reached the
    OS -- its completion offset has been passed by `_cumulative_sent` -- in
    send order. Once the queue is fully drained and empty, reset the byte
    counters. They only grow while the queue never fully empties; on a 64-bit
    target that won't overflow for any real transfer.
    """
    try
      while _pending_tokens.size() > 0 do
        (let offset, let token) = _pending_tokens(0)?
        if offset > _cumulative_sent then break end
        _pending_tokens.shift()?
        _fire_on_sent(token)
      end
    else
      // Guarded by size() > 0, so the ? accesses never error.
      _Unreachable()
    end
    if (_pending_tokens.size() == 0) and (_pending.total() == 0) then
      _cumulative_enqueued = 0
      _cumulative_sent = 0
    end

  fun _tcp_buffer_until(): (BufferSize | Streaming) =>
    """
    The buffer-until value for the TCP read layer. When SSL is active, returns
    `Streaming` because SSL record framing doesn't align with application
    framing — the TCP layer reads all available data and lets the SSL session
    frame via `_buffer_until`. When SSL is not active, returns the user's
    `_buffer_until` value directly.
    """
    // Only a plaintext connection frames in the read buffer. Every TLS variant
    // stages ciphertext there, or nothing at all.
    match _ssl
    | _NoTLS => _buffer_until
    else
      Streaming
    end

  fun ref _next_message():
    (Array[U8] iso^ | None | SSLClosed | SSLError | InvalidOperation)
  =>
    """
    The next message for the application, or `None` when there isn't one.

    Returns a value; it never calls the application. That is what keeps `mute`
    in `_read()`'s loop and out of here — code that hands over control needs
    that guard, code that hands back a value does not.

    For an SSL connection the messages come from the SSL session, which frames
    them itself. Otherwise they are chopped off the read buffer.
    """
    match \exhaustive\ _ssl
    | let tls: _TLS =>
      tls.session.read(_user_buffer_until())
    | _TLSDisposed | _TLSFailed =>
      None
    | _NoTLS =>
      if not _there_is_buffered_read_data() then
        return None
      end

      let bytes_to_consume =
        match \exhaustive\ _buffer_until
        | let e: BufferSize => e()
        | Streaming => _bytes_in_read_buffer
        end

      let x = _read_buffer = recover Array[U8] end
      (let data', _read_buffer) = (consume x).chop(bytes_to_consume)
      _bytes_in_read_buffer = _bytes_in_read_buffer - bytes_to_consume
      consume data'
    end

  fun ref _fill(s: EitherLifecycleEventReceiver[TCP] ref): (USize | None) ? =>
    """
    Get more bytes off the socket. Returns the number read, or `None` when the
    socket has nothing more to give (read interest is re-armed first). Raises
    when the read fails — a peer close, or an unrecoverable socket error.
    `_read()` hard closes on either.

    The only place that knows whether this connection is using SSL. For SSL the
    bytes go to the session rather than to the application, and `_ssl_poll()`
    handles handshake state, errors, and flushing.

    Returns as soon as the session has been fed. `_ssl_poll()` runs application
    callbacks that can `hard_close()` and dispose the session, so the next look
    at it has to happen after `_read()`'s loop re-checks its guards.
    """
    _resize_read_buffer_if_needed()

    let bytes_read =
      match \exhaustive\ _state.receive(
        this,
        _event,
        _read_buffer.cpointer(_bytes_in_read_buffer),
        _read_buffer.size() - _bytes_in_read_buffer)
      | (SocketResultOk, let n: USize) => n
      | (SocketResultRetry, _) =>
        _set_unreadable()
        PonyAsio.resubscribe_read(_event)
        return None
      | (SocketResultError, _) => error
      end

    _bytes_in_read_buffer = _bytes_in_read_buffer + bytes_read

    match _ssl
    | let tls: _TLS =>
      let x = _read_buffer = recover Array[U8] end
      (let cipher, _read_buffer) = (consume x).chop(_bytes_in_read_buffer)
      _bytes_in_read_buffer = 0
      let ssl_result = tls.session.receive(consume cipher)
      _ssl_poll(s, ssl_result)
    end

    bytes_read

  fun ref _read() =>
    _reset_idle_timer()
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      try
        var total_bytes_read: USize = 0

        while _readable do
          if _muted then
            // Mute stops reading. It does not hold the write side, so protocol
            // output `ssl.read()` queued still goes out; a mute lasts as long
            // as the application likes.
            _ssl_flush_sends()
            return
          end

          match \exhaustive\ _next_message()
          | let m: Array[U8] iso =>
            match \exhaustive\ s._on_received(consume m)
            | KeepReading => None
            | YieldReading =>
              _queue_read()
              return
            end
          | SSLClosed =>
            if _shutdown then
              // Both sides have exchanged close_notify, and we have already
              // sent FIN. The TLS session will keep returning SSLClosed on
              // every read from here on, so continuing to read would loop
              // forever. Tear down the socket.
              hard_close()
            else
              close()
              if not PonyAsio.get_disposable(_event) then
                _set_unreadable()
                PonyAsio.resubscribe_read(_event)
              end
            end
            return
          | SSLError =>
            hard_close()
            return
          | InvalidOperation =>
            _Unreachable()
            return
          | None =>
            // Reading the SSL session in `_next_message()` can make it queue
            // protocol output — a TLS 1.3 KeyUpdate response, say. The session
            // has drained now, so flush that before blocking on the socket; a
            // peer waiting on the output would otherwise wedge. No-op on a
            // plaintext connection.
            _ssl_flush_sends()
            if _muted then
              return
            end

            // Yield after reading a buffer's worth of data to allow GC and
            // other actors to run.
            if total_bytes_read >= _read_buffer_size then
              _queue_read()
              return
            end

            match \exhaustive\ _fill(s)?
            | let n: USize => total_bytes_read = total_bytes_read + n
            | None => return
            end
          end
        end
      else
        hard_close()
      end
    | None =>
      _Unreachable()
    end

  fun _there_is_buffered_read_data(): Bool =>
    match \exhaustive\ _tcp_buffer_until()
    | let e: BufferSize => _bytes_in_read_buffer >= e()
    | Streaming => _bytes_in_read_buffer > 0
    end

  fun ref _read_buffer_free_space(): USize =>
    """
    Bytes the read buffer has room for before it has to grow. Zero means the
    next read has nowhere to put what it gets.
    """
    _read_buffer.size() - _bytes_in_read_buffer

  fun ref _resize_read_buffer_if_needed() =>
    """
    Resize the read buffer if it's smaller than the buffer-until threshold, or
    shrink it back to the minimum when empty and oversized.
    """
    let needs_grow =
      match \exhaustive\ _tcp_buffer_until()
      | let e: BufferSize => _read_buffer.size() <= e()
      | Streaming => _read_buffer.size() == 0
      end
    if needs_grow then
      _read_buffer.undefined(_read_buffer_size)
    elseif (_bytes_in_read_buffer == 0) and
      (_read_buffer_size > _read_buffer_min)
    then
      _read_buffer_size = _read_buffer_min
      _read_buffer =
        recover iso
          Array[U8](_read_buffer_size) .> undefined(_read_buffer_size)
        end
    end

  fun ref _queue_read() =>
    """
    Schedule reading to resume in a later turn via the `_read_again` behavior.
    Used when `_on_received` returns `YieldReading`, to yield after a buffer's
    worth of data, and to (re)start reading after establishing a connection or
    unmuting.
    """
    match \exhaustive\ _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      e._read_again()
    | None =>
      _Unreachable()
    end

  fun ref _apply_backpressure() =>
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] =>
      if not _throttled then
        _throttled = true
        // throttled means we are also unwriteable
        // being unthrottled doesn't however mean we are writable
        _set_unwriteable()
        PonyAsio.resubscribe_write(_event)
        // A hard close from the application fails every token still on the
        // queue, so report the sends this flush has completed before the
        // application runs. The `_on_sent` that reports them can itself
        // `hard_close()`, which clears `_throttled`, so re-check the flag:
        // `_on_throttled` must not follow an `_on_closed`.
        _fire_completed_sends()
        if _throttled then
          s._on_throttled()
        end
      end
    | None =>
      _Unreachable()
    end

  fun ref _release_backpressure() =>
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] =>
      if _throttled then
        _throttled = false
        s._on_unthrottled()
      end
    | None =>
      _Unreachable()
    end

  fun ref _fire_on_send_accepted(token: SendToken,
    data: (ByteSeq | ByteSeqIter))
  =>
    """
    Dispatch _on_send_accepted to the lifecycle event receiver.
    """
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_send_accepted(token, data)
    | None =>
      _Unreachable()
    end

  fun ref _fire_on_sent(token: SendToken) =>
    """
    Dispatch _on_sent to the lifecycle event receiver.
    """
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_sent(token)
    | None =>
      _Unreachable()
    end

  fun ref _fire_on_send_failed(token: SendToken) =>
    """
    Dispatch _on_send_failed to the lifecycle event receiver. Called from
    _notify_send_failed behavior on TCPConnectionActor.
    """
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_send_failed(token)
    | None =>
      _Unreachable()
    end

  fun ref _do_idle_timeout(duration: (IdleTimeout | None)) =>
    match \exhaustive\ duration
    | let t: IdleTimeout =>
      _idle_timeout_nsec = t() * 1_000_000
      if _timer_event.is_null() then
        _arm_idle_timer()
      else
        _reset_idle_timer()
      end
    | None =>
      _idle_timeout_nsec = 0
      _cancel_idle_timer()
    end

  fun ref _store_idle_timeout(duration: (IdleTimeout | None)) =>
    match \exhaustive\ duration
    | let t: IdleTimeout =>
      _idle_timeout_nsec = t() * 1_000_000
    | None =>
      _idle_timeout_nsec = 0
    end

  fun ref _do_set_timer(duration: TimerDuration):
    (TimerToken | SetTimerError)
  =>
    if _user_timer_token isnt None then return SetTimerAlreadyActive end

    let nsec = duration() * 1_000_000
    match \exhaustive\ _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      _user_timer_event = PonyAsio.create_timer_event(e, nsec)
    | None =>
      _Unreachable()
    end
    let token = TimerToken._create(_next_timer_id = _next_timer_id + 1)
    _user_timer_token = token
    token

  fun ref _arm_idle_timer() =>
    """
    Create the ASIO timer event for idle timeout. Called when the connection
    establishes and `_idle_timeout_nsec > 0`, or when `idle_timeout()` is
    called on an established connection.

    Idempotent — if a timer already exists, this is a no-op. Prevents ASIO
    timer event leaks from double-arm scenarios.
    """
    if _idle_timeout_nsec == 0 then return end
    if not _timer_event.is_null() then return end
    match \exhaustive\ _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      _timer_event = PonyAsio.create_timer_event(e, _idle_timeout_nsec)
    | None =>
      _Unreachable()
    end

  fun ref _reset_idle_timer() =>
    """
    Reset the idle timer to the configured duration. Called on I/O activity:
    a successful `sendv` (an application send or a buffered-write drain) or
    data received. Only resets an existing timer — does not create one.
    """
    if not _timer_event.is_null() then
      PonyAsio.set_timer(_timer_event, _idle_timeout_nsec)
    end

  fun ref _cancel_idle_timer() =>
    """
    Cancel the idle timer. Unsubscribes and clears `_timer_event`
    immediately. The stale disposable notification (if any) no longer
    matches `_timer_event` and is destroyed by `_event_notify`'s else
    branch disposable check.
    """
    if not _timer_event.is_null() then
      PonyAsio.unsubscribe(_timer_event)
      _timer_event = AsioEvent.none()
      _idle_timeout_nsec = 0
    end

  fun ref _fire_idle_timeout() =>
    _state.fire_idle_timeout(this)

  fun ref _dispatch_idle_timeout() =>
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_idle_timeout()
    | None =>
      _Unreachable()
    end

  fun ref _rearm_idle_timer_if_configured() =>
    if _idle_timeout_nsec > 0 then
      _reset_idle_timer()
    end

  fun ref _fire_idle_timer_failure() =>
    """
    The idle timer's ASIO subscription failed. Cancel the timer (which
    unsubscribes the event and zeroes `_idle_timeout_nsec`), then dispatch
    `_on_idle_timer_failure` to the lifecycle event receiver. Cancelling
    before dispatch means the callback can call `idle_timeout(duration)`
    to re-arm without hitting the idempotency guard in `_arm_idle_timer`.
    """
    _cancel_idle_timer()
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_idle_timer_failure()
    | None =>
      _Unreachable()
    end

  fun ref _arm_connect_timer() =>
    """
    Create the ASIO timer event for the connect timeout. Called after
    connect succeeds (at least one connection attempt is inflight).
    No-op when `_connect_timeout_nsec == 0` (no timeout configured).
    """
    if _connect_timeout_nsec == 0 then return end
    match \exhaustive\ _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      _connect_timer_event =
        PonyAsio.create_timer_event(e, _connect_timeout_nsec)
    | None =>
      _Unreachable()
    end

  fun ref _cancel_connect_timer() =>
    """
    Cancel the connect timeout timer. Unsubscribes and clears
    `_connect_timer_event` immediately. Stale disposable notifications
    no longer match `_connect_timer_event` and are destroyed by
    `_event_notify`'s else branch disposable check.
    """
    if not _connect_timer_event.is_null() then
      PonyAsio.unsubscribe(_connect_timer_event)
      _connect_timer_event = AsioEvent.none()
      _connect_timeout_nsec = 0
    end

  fun ref _fire_connect_timeout() =>
    """
    The connect timeout has fired. Cancels the timer and hard-closes,
    saying why, so the connection fails with `ConnectionFailedTimeout`.
    """
    _cancel_connect_timer()
    _hard_close(_ConnectTimedOut)

  fun ref _fire_connect_timer_error() =>
    """
    The connect timer's ASIO subscription failed. Cancels the timer and
    hard-closes, saying why, so the connection fails with
    `ConnectionFailedTimerError`.
    """
    _cancel_connect_timer()
    _hard_close(_ConnectTimerFailed)

  fun ref _fire_user_timer() =>
    """
    Dispatch `_on_timer` to the lifecycle event receiver. Called from
    `_event_notify` when the user timer event fires.

    The token and event are cleared before the callback. If the callback
    calls `set_timer()`, it creates a fresh ASIO event. The old event's
    disposable notification arrives later, doesn't match
    `_user_timer_event`, and is destroyed by `_event_notify`'s else
    branch disposable check.
    """
    let token = _user_timer_token
    _user_timer_token = None
    PonyAsio.unsubscribe(_user_timer_event)
    _user_timer_event = AsioEvent.none()
    match \exhaustive\ (token, _lifecycle_event_receiver)
    | (let t: TimerToken, let s: EitherLifecycleEventReceiver[TCP] ref) =>
      s._on_timer(t)
    | (None, _) =>
      _Unreachable()
    | (_, None) =>
      _Unreachable()
    end

  fun ref _cancel_user_timer() =>
    """
    Cancel the user timer without firing the callback. Called from both
    hard-close paths during cleanup. Stale disposable notifications no
    longer match `_user_timer_event` and are destroyed by
    `_event_notify`'s else branch disposable check.
    """
    if not _user_timer_event.is_null() then
      PonyAsio.unsubscribe(_user_timer_event)
      _user_timer_event = AsioEvent.none()
      _user_timer_token = None
    end

  fun ref _fire_user_timer_failure() =>
    """
    The user timer's ASIO subscription failed. Cancel the timer (which
    unsubscribes the event and clears `_user_timer_token`), then dispatch
    `_on_timer_failure` to the lifecycle event receiver. Cancelling before
    dispatch means the callback can call `set_timer(duration)` to create
    a new timer without hitting the `SetTimerAlreadyActive` guard.
    """
    _cancel_user_timer()
    match \exhaustive\ _lifecycle_event_receiver
    | let s: EitherLifecycleEventReceiver[TCP] ref =>
      s._on_timer_failure()
    | None =>
      _Unreachable()
    end

  fun ref _ssl_enqueue_sends() =>
    """
    Drain pending encrypted data from the SSL session into the write queue,
    without flushing. Split out from `_ssl_flush_sends` so `_do_send` can
    record a send's completion offset after its ciphertext is enqueued but
    before the flush.
    """
    match _ssl
    | let tls: _TLS =>
      while true do
        match \exhaustive\ tls.session.send()
        | let data: Array[U8] iso =>
          _enqueue(consume data)
        | None => break
        end
      end
    end

  fun ref _ssl_flush_sends() =>
    """
    Enqueue any pending encrypted data from the SSL session, then flush to the
    wire. Called after SSL handshake and protocol operations that produce
    output (ClientHello, handshake responses), and by `_read()` once
    `ssl.read()` may have queued some — a TLS 1.3 KeyUpdate response, say.

    Does nothing without a live session. `_ssl_poll()` runs callbacks that can
    `hard_close()` and dispose it, and there is then nothing to flush and no
    socket to flush it to.

    Can `hard_close()` on a write error. The flush also runs `_on_sent` and
    `_on_throttled`, either of which can `hard_close()` from the application.
    The application write path uses `_ssl_enqueue_sends()` plus
    `_send_pending_writes()` directly so it can record the send's completion
    offset between the two.
    """
    match _ssl
    | let _: _TLS =>
      _ssl_enqueue_sends()
      _send_pending_writes()
    end

  fun ref _ssl_poll(
    s: EitherLifecycleEventReceiver[TCP] ref,
    result: SSLReceiveResult)
  =>
    """
    Handle handshake completion, error detection, and protocol data flushing
    for the SSL session. Called by `_fill()` after `ssl.receive()` has fed it
    new ciphertext and returned a result.

    Does not deliver application data — `_read()` takes messages out of the
    session one at a time via `_next_message()`.

    `ssl_handshake_complete` runs application callbacks, any of which can
    `hard_close()` and dispose the session. The flush below re-matches `_ssl`
    rather than reusing a binding, so it finds no session and does nothing.
    """
    match \exhaustive\ result
    | SSLAccepted => None
    | SSLReady =>
      _state.ssl_handshake_complete(this, s)
    | SSLAuthFail =>
      _hard_close(_TLSAuthFailure)
      return
    | SSLError =>
      hard_close()
      return
    | InvalidOperation =>
      _Unreachable()
      return
    end
    _ssl_flush_sends()

  fun _has_pending_writes(): Bool =>
    _pending.total() > 0

  fun ref read_again() =>
    _state.read_again(this)

  fun ref _dispatch_io_event(flags: U32) =>
    """
    Common I/O dispatch logic for socket events. Shared by all states that
    have a connected socket and need to process I/O notifications. Identical
    on every platform: readiness edges drive synchronous recv/sendv.
    """
    if AsioEvent.errored(flags) then
      hard_close()
      return
    end

    if AsioEvent.writeable(flags) then
      _set_writeable()
      _send_pending_writes()
    end

    if AsioEvent.readable(flags) then
      _set_readable()
      _read()
    else
      // A whole-fd one-shot event (Linux epoll, Windows readiness) disarms the
      // fd, so a write-only event drops read interest. Re-arm reads, guarded by
      // `_writeable` to skip a closed or backpressured fd. Do not weaken; the
      // reasoning and the deadlock it prevents are in #294, #296.
      //
      // kqueue fires EVFILT_READ and EVFILT_WRITE as independent one-shot
      // filters: a write event does not disarm read interest, so the re-arm
      // is unnecessary there. On BSDs the redundant resubscribe also triggers
      // a retry_loop pipe write, doubling wakeup traffic per backpressure
      // cycle; skip it there.
      ifdef not bsd then
        if _writeable and not PonyAsio.get_disposable(_event) then
          PonyAsio.resubscribe_read(_event)
        end
      end
    end

    // Mirror for the write side: a readable event drops the write interest, and
    // a read that mutes or yields before EAGAIN never re-arms it, so a
    // backpressured write wedges. Re-arm it, guarded by `_throttled`.
    // Do not weaken; see #294, #296.
    //
    // Same as above: kqueue's independent filters make this unnecessary
    // on BSDs, and the redundant resubscribe adds retry_loop overhead.
    ifdef not bsd then
      if _throttled and not PonyAsio.get_disposable(_event) then
        PonyAsio.resubscribe_write(_event)
      end
    end

  fun ref _do_read_again() =>
    _read()

  fun ref _set_state(state: _ConnectionState[TCP] ref) =>
    _state = state

  fun _has_inflight_events(): Bool =>
    _inflight_events.size() > 0

  fun ref _establish_connection(event: AsioEventID, fd: U32) =>
    """
    Called by _ClientConnecting when a Happy Eyeballs connection succeeds.
    Promotes the event to the connection's own event, transitions to the
    appropriate state, and sets up the connection for I/O.
    """
    _remove_inflight_event(event)
    _event = event
    _fd = fd
    _set_writeable()
    _set_readable()

    match \exhaustive\ _ssl
    | let _: _TLS =>
      _state = _SSLHandshaking[TCP]
      // Flush ClientHello to initiate SSL handshake.
      // _on_connected() and _arm_idle_timer() deferred until
      // ssl_handshake_complete.
      _ssl_flush_sends()
    | _TLSDisposed | _TLSFailed =>
      _Unreachable()
    | _NoTLS =>
      _state = _Open[TCP]
      _arm_idle_timer()
      _cancel_connect_timer()
      match _lifecycle_event_receiver
      | let c: ClientLifecycleEventReceiver[TCP] ref =>
        c._on_connected()
      end
    end

    _read()
    if _has_pending_writes() then
      _send_pending_writes()
    end

  fun ref _close_raw_fd() =>
    """
    Close the connection's fd directly, bypassing `_close_event_fd`. For use
    when no ASIO event was ever created for this fd.
    """
    _tcp.close(_fd)
    _fd = -1

  fun ref _close_event_fd(fd: U32) =>
    """
    Close the fd backing a subscribed event. On POSIX the stdlib owns the
    close (the readiness backend never owns fds). On Windows the readiness
    backend owns it: the fd is closed when the deferred
    ProcessSocketNotifications REMOVE (issued by the unsubscribe) is seen, so
    closing here would emit no REMOVE and strand the disposal handshake,
    leaking the fd and event.

    Use this for every fd whose event was created via
    `pony_asio_event_create`. Raw, never-subscribed fds (e.g. an accepted fd
    rejected before an event is created) are closed directly with
    `close` on both platforms.
    """
    ifdef not windows then
      _tcp.close(fd)
    end

  fun ref _connecting_event_failed(event: AsioEventID, fd: U32) =>
    """
    Called by _ClientConnecting when a Happy Eyeballs connection attempt
    fails. Unsubscribes the event, closes the fd, and fires the connecting
    callback.
    """
    _remove_inflight_event(event)
    PonyAsio.unsubscribe(event)
    _close_event_fd(fd)
    _connecting_callback()

  fun ref _straggler_cleanup(event: AsioEventID) =>
    """
    Clean up a Happy Eyeballs straggler event after the winner has been
    chosen. Unsubscribes the event and closes the fd.
    """
    _remove_inflight_event(event)
    PonyAsio.unsubscribe(event)
    _close_event_fd(PonyAsio.event_fd(event))

  fun ref _cancel_inflight_events() =>
    """
    Unsubscribe and close all tracked inflight connecting events. Called
    during hard close to cancel pending OS connect attempts whose ASIO
    events have not yet delivered a message to this actor.
    """
    for ev in _inflight_events.values() do
      if not ev.is_null() then
        PonyAsio.unsubscribe(ev)
        _close_event_fd(PonyAsio.event_fd(ev))
      end
    end
    _inflight_events.clear()

  fun ref _remove_inflight_event(event: AsioEventID) =>
    try
      var i: USize = 0
      while i < _inflight_events.size() do
        if _inflight_events(i)? is event then
          _inflight_events.delete(i)?
          return
        end
        i = i + 1
      end
    end

  fun ref _event_notify(event: AsioEventID, flags: U32) =>
    // Explicit dispatch on event identity. Timer identity checks must come
    // before `event is _event`.
    if event is _connect_timer_event then
      if AsioEvent.errored(flags) then
        _fire_connect_timer_error()
      else
        _fire_connect_timeout()
      end
    elseif event is _timer_event then
      if AsioEvent.errored(flags) then
        _fire_idle_timer_failure()
      else
        _fire_idle_timeout()
      end
    elseif event is _user_timer_event then
      if AsioEvent.errored(flags) then
        _fire_user_timer_failure()
      else
        _fire_user_timer()
      end
    elseif event is _event then
      _state.own_event(this, flags)
      if AsioEvent.disposable(flags) then
        PonyAsio.destroy(event)
        _event = AsioEvent.none()
      end
    else
      if AsioEvent.disposable(flags) then
        PonyAsio.destroy(event)
      elseif PonyAsio.get_disposable(event) then
        // The message and the event struct disagree: this carries live flags
        // for an event that has already been unsubscribed. Whatever the
        // message is for was done when the event was unsubscribed, so there is
        // nothing left to act on, and acting anyway means a second cleanup of
        // an fd the OS may have reassigned and a second removal from the
        // inflight array. Drop it rather than destroy it -- the unsubscribe
        // sends a disposable notification of its own, and that one frees the
        // struct.
        None
      else
        _state.foreign_event(this, event, flags)
      end
    end

  fun ref _connecting_callback() =>
    match \exhaustive\ _lifecycle_event_receiver
    | let c: ClientLifecycleEventReceiver[TCP] ref =>
      if _has_inflight_events() then
        c._on_connecting(_inflight_events.size().u32())
      else
        hard_close()
      end
    | let s: ServerLifecycleEventReceiver[TCP] ref =>
      _Unreachable()
    | None =>
      _Unreachable()
    end

  fun _is_socket_connected(fd: U32): Bool =>
    (let errno: U32, let value: U32) = _OSSocket.get_so_error(fd)
    (errno == 0) and (value == 0)

  fun ref _register_spawner(listener: TCPListenerActor[TCP]) =>
    if _spawned_by is None then
      if not _state.is_closed() then
        // We were connected by the time the spawner was registered,
        // so, let's let it know we were connected
        _spawned_by = listener
      else
        // We were closed by the time the spawner was registered,
        // so, let's let it know we were closed, And leave our "spawned by" as
        // None.
        listener._connection_closed()
      end
    else
      _Unreachable()
    end

  fun ref _finish_initialization() =>
    // _finish_initialization is a self→self message queued during the
    // constructor. dispose() comes from an external actor. Different senders
    // have no ordering guarantee, so dispose() can race ahead and transition
    // to _Closed via _ConnectionNone.hard_close(). When that happens, the fd
    // and TLS session are already cleaned up — skip initialization.
    match _state
    | let _: _Closed[TCP] => return
    end

    match \exhaustive\ _lifecycle_event_receiver
    | let s: ServerLifecycleEventReceiver[TCP] ref =>
      _complete_server_initialization(s)
    | let c: ClientLifecycleEventReceiver[TCP] ref =>
      _complete_client_initialization(c)
    | None =>
      _Unreachable()
    end

  fun ref _complete_client_initialization(
    s: ClientLifecycleEventReceiver[TCP] ref)
  =>
    if _ssl is _TLSFailed then
      _state = _Closed[TCP]
      s._on_connection_failure(ConnectionFailedSSL)
      return
    end

    match \exhaustive\ _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      _state = _ClientConnecting[TCP]

      _inflight_events =
        _tcp.connect(
          e, _host, _port, _from, AsioEvent.read_write_oneshot()
          where ip_version = _ip_version)
      _had_inflight = _inflight_events.size() > 0
      if _had_inflight then
        _arm_connect_timer()
      end
      _connecting_callback()
    | None =>
      _Unreachable()
    end

  fun ref _complete_server_initialization(
    s: ServerLifecycleEventReceiver[TCP] ref)
  =>
    if _ssl is _TLSFailed then
      // Raw fd: no ASIO event has been created for it yet (that happens below,
      // after this early return), so close it directly on every platform. Do
      // NOT route this through `_close_event_fd` — that defers to the readiness
      // backend on Windows, which would never close this never-subscribed fd.
      _tcp.close(_fd)
      _fd = -1
      _state = _Closed[TCP]
      s._on_start_failure(StartFailedSSL)
      return
    end

    match \exhaustive\ _enclosing
    | let e: TCPConnectionActor[TCP] ref =>
      _event = PonyAsio.create_event(e, _fd)
      _set_readable()
      _set_writeable()

      match \exhaustive\ _ssl
      | let _: _TLS =>
        _state = _SSLHandshaking[TCP]
        // Flush any initial SSL data (usually no-op for servers).
        // _on_started() and _arm_idle_timer() deferred until
        // ssl_handshake_complete.
        _ssl_flush_sends()
      | _TLSDisposed | _TLSFailed =>
        _Unreachable()
      | _NoTLS =>
        _state = _Open[TCP]
        _arm_idle_timer()
        s._on_started()
      end

      _queue_read()
    | None =>
      _Unreachable()
    end

  fun ref _set_readable() =>
    _readable = true
    PonyAsio.set_readable(_event)

  fun ref _set_unreadable() =>
    _readable = false
    PonyAsio.set_unreadable(_event)

  fun ref _set_writeable() =>
    _writeable = true
    PonyAsio.set_writeable(_event)
    _release_backpressure()

  fun ref _set_unwriteable() =>
    _writeable = false
    PonyAsio.set_unwriteable(_event)
