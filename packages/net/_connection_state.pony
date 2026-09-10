
trait _ConnectionState[TCP: TCPBackend ref]
  """
  One state in the connection lifecycle. `TCPConnection._state` holds the
  current one, and lifecycle-gated operations dispatch through it: each state
  answers what happens in it, and delegates the actual work to `TCPConnection`.
  """
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32)
    """
    Handle an ASIO event for this connection's own socket event.
    """

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
    """
    Handle an ASIO event that is not this connection's socket event (a Happy
    Eyeballs straggler).
    """

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter))
    : SendResult
    """
    Send data, or return why it can't be sent in this state.
    """

  fun ref drained(conn: TCPConnection[TCP] ref)
    """
    The pending write queue is empty. A state that defers work until then
    does it here, and re-checks anything it depends on: an application
    callback can run between the queue emptying and this call.
    """

  fun ref close(conn: TCPConnection[TCP] ref)
    """
    Graceful close from this state.
    """

  fun ref hard_close(conn: TCPConnection[TCP] ref, cause: _HardCloseCause)
    """
    Non-graceful close from this state, routing `cause` to a failure callback.
    """

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
    """
    Upgrade to TLS, or return why it can't happen in this state.
    """

  fun ref read_again(conn: TCPConnection[TCP] ref)
    """
    Resume reading after a yield, if this state still reads.
    """

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
    """
    The SSL session reached `SSLReady`. Only the handshake states act.
    """

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32)
    """
    Set TCP keepalive, if the socket is open in this state.
    """

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
    """
    Raw `getsockopt`, or an error value if not open in this state.
    """

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
    """
    `getsockopt` for a U32, or an error value if not open in this state.
    """

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
    """
    Raw `setsockopt`, or an error value if not open in this state.
    """

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
    """
    `setsockopt` for a U32, or an error value if not open in this state.
    """

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
    """
    Set or clear the idle timeout; states differ in whether they arm it.
    """

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref)
    """
    The idle timer fired. Dispatch the callback and re-arm if this state
    should keep the timer running.
    """

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration)
    : (TimerToken | SetTimerError)
    """
    Start a user timer, or return why it can't be started in this state.
    """

  fun is_closed(): Bool
    """
    The connection is closed or closing.
    """

  fun sends_allowed(): Bool
    """
    Sends are accepted in this state.
    """

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
    """
    Read from the socket. Only states that can receive perform it.
    """

class _ConnectionNone[TCP: TCPBackend ref] is _ConnectionState[TCP]
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    _Unreachable()

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    _Unreachable()

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    _Unreachable()
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) =>
    _Unreachable()

  fun ref close(conn: TCPConnection[TCP] ref) =>
    _Unreachable()

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    // _finish_initialization is a self→self message queued during the
    // constructor. dispose() comes from an external actor. Different senders
    // have no ordering guarantee, so dispose() can arrive first — unlikely
    // but possible. Transition to _Closed so _finish_initialization (which
    // will still run) sees it and skips ASIO event creation. No ASIO event
    // exists yet, so close the raw fd directly and dispose TLS.
    conn._set_state(_Closed[TCP])
    conn._close_raw_fd()
    conn._dispose_tls()

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    _Unreachable()
    StartTLSNotConnected

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    _Unreachable()

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    _Unreachable()

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) => None

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._store_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    SetTimerNotOpen

  fun is_closed(): Bool => false
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    _Unreachable()
    (SocketResultError, 0)

class _ClientConnecting[TCP: TCPBackend ref] is _ConnectionState[TCP]
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    _Unreachable()

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    // Check errored before the writeable/readable guard. An errored event
    // must NOT flow into _is_socket_connected — the FD might appear
    // "connected" via getsockopt(SO_ERROR) even though its ASIO subscription
    // is broken.
    if AsioEvent.errored(flags) then
      let fd = PonyAsio.event_fd(event)
      conn._connecting_event_failed(event, fd)
      return
    end

    if not (AsioEvent.writeable(flags) or AsioEvent.readable(flags)) then
      return
    end

    let fd = PonyAsio.event_fd(event)

    if conn._is_socket_connected(fd) then
      conn._establish_connection(event, fd)
    else
      conn._connecting_event_failed(event, fd)
    end

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) =>
    _Unreachable()

  fun ref close(conn: TCPConnection[TCP] ref) =>
    conn._set_state(_UnconnectedClosing[TCP])

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    conn._hard_close_connecting(cause)

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    StartTLSNotConnected

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    None

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    _Unreachable()

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) => None

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._store_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    SetTimerNotOpen

  fun is_closed(): Bool => false
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    _Unreachable()
    (SocketResultError, 0)

class _Open[TCP: TCPBackend ref] is _ConnectionState[TCP]
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    conn._dispatch_io_event(flags)

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    if not (AsioEvent.errored(flags) or AsioEvent.writeable(flags) or
      AsioEvent.readable(flags))
    then
      return
    end

    // Happy Eyeballs straggler — clean up
    conn._straggler_cleanup(event)

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    conn._do_send(data)

  fun ref drained(conn: TCPConnection[TCP] ref) => None

  fun ref close(conn: TCPConnection[TCP] ref) =>
    conn._set_state(_Closing[TCP])
    conn._cancel_idle_timer()
    conn._mark_close_notify_pending()
    conn._initiate_shutdown()

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    conn._hard_close_connected()

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    conn._do_start_tls(ssl_ctx, host)

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    conn._do_read_again()

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    // Already open: the handshake completed on the way in.
    None

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) =>
    conn._do_keepalive(secs)

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    conn._do_getsockopt(level, option_name, option_max_size)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    conn._do_getsockopt_u32(level, option_name)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    conn._do_setsockopt(level, option_name, option)

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    conn._do_setsockopt_u32(level, option_name, option)

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._do_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()
    conn._rearm_idle_timer_if_configured()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    conn._do_set_timer(duration)

  fun is_closed(): Bool => false
  fun sends_allowed(): Bool => true

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    conn._tcp_ops().receive(event, buffer, size)

class _Closing[TCP: TCPBackend ref] is _ConnectionState[TCP]
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    // No trailing `_initiate_shutdown()`: the FIN waits on the write queue
    // emptying, and `drained` is where that becomes true.
    conn._dispatch_io_event(flags)

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    if not (AsioEvent.errored(flags) or AsioEvent.writeable(flags) or
      AsioEvent.readable(flags))
    then
      return
    end

    // Happy Eyeballs straggler — clean up
    conn._straggler_cleanup(event)

    // Inflight drained — try to advance the shutdown sequence
    conn._initiate_shutdown()

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) =>
    conn._close_notify_then_shutdown()

  fun ref close(conn: TCPConnection[TCP] ref) =>
    None

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    conn._hard_close_connected()

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    StartTLSNotConnected

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    conn._do_read_again()

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    // Already open: the handshake completed on the way in.
    None

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) => None

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._store_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    SetTimerNotOpen

  fun is_closed(): Bool => true
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    conn._tcp_ops().receive(event, buffer, size)

class _UnconnectedClosing[TCP: TCPBackend ref] is _ConnectionState[TCP]
  """
  Draining inflight Happy Eyeballs connections after close() during the
  connecting phase. The failure callback is deferred until all inflight
  connections drain. hard_close() can interrupt the drain, cancelling all
  remaining events immediately.
  """
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    _Unreachable()

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    if not (AsioEvent.errored(flags) or AsioEvent.writeable(flags) or
      AsioEvent.readable(flags))
    then
      return
    end

    conn._straggler_cleanup(event)

    if not conn._has_inflight_events() then
      conn._hard_close_connecting(_UnspecifiedCause)
    end

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) =>
    _Unreachable()

  fun ref close(conn: TCPConnection[TCP] ref) =>
    None

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    conn._hard_close_connecting(cause)

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    StartTLSNotConnected

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    None

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    _Unreachable()

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) => None

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._store_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    SetTimerNotOpen

  fun is_closed(): Bool => true
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    _Unreachable()
    (SocketResultError, 0)

class _Closed[TCP: TCPBackend ref] is _ConnectionState[TCP]
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    None

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    if not (AsioEvent.errored(flags) or AsioEvent.writeable(flags) or
      AsioEvent.readable(flags))
    then
      return
    end

    // Happy Eyeballs straggler — clean up
    conn._straggler_cleanup(event)

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) => None

  fun ref close(conn: TCPConnection[TCP] ref) =>
    None

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    None

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    StartTLSNotConnected

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    None

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    _Unreachable()

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) => None

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._store_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    SetTimerNotOpen

  fun is_closed(): Bool => true
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultError, 0)

class _SSLHandshaking[TCP: TCPBackend ref] is _ConnectionState[TCP]
  """
  TCP connected, initial SSL handshake in progress. The application has not
  been notified yet — `_on_connected`/`_on_started` fires only after the
  handshake completes.
  """
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    conn._dispatch_io_event(flags)

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    if not (AsioEvent.errored(flags) or AsioEvent.writeable(flags) or
      AsioEvent.readable(flags))
    then
      return
    end

    // Happy Eyeballs straggler — clean up
    conn._straggler_cleanup(event)

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) => None

  fun ref close(conn: TCPConnection[TCP] ref) =>
    // Can't drain gracefully during handshake — nothing to FIN.
    conn.hard_close()

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    conn._hard_close_ssl_handshaking(cause)

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    StartTLSNotConnected

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    conn._do_read_again()

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    conn._set_state(_Open[TCP])
    conn._cancel_connect_timer()
    conn._arm_idle_timer()
    match \exhaustive\ s
    | let c: ClientLifecycleEventReceiver[TCP] ref =>
      c._on_connected()
    | let srv: ServerLifecycleEventReceiver[TCP] ref =>
      srv._on_started()
    end

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) => None

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._store_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    SetTimerNotOpen

  fun is_closed(): Bool => false
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    conn._tcp_ops().receive(event, buffer, size)

class _TLSUpgrading[TCP: TCPBackend ref] is _ConnectionState[TCP]
  """
  Established connection upgrading to TLS via `start_tls()`. The application
  has already been notified of the plaintext connection — `_on_tls_ready`
  fires when the handshake completes.
  """
  fun ref own_event(conn: TCPConnection[TCP] ref, flags: U32) =>
    conn._dispatch_io_event(flags)

  fun ref foreign_event(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    flags: U32)
  =>
    if not (AsioEvent.errored(flags) or AsioEvent.writeable(flags) or
      AsioEvent.readable(flags))
    then
      return
    end

    // Happy Eyeballs straggler — clean up
    conn._straggler_cleanup(event)

  fun ref send(conn: TCPConnection[TCP] ref,
    data: (ByteSeq | ByteSeqIter)): SendResult
  =>
    SendErrorNotConnected

  fun ref drained(conn: TCPConnection[TCP] ref) => None

  fun ref close(conn: TCPConnection[TCP] ref) =>
    // Can't send FIN during TLS handshake.
    conn.hard_close()

  fun ref hard_close(conn: TCPConnection[TCP] ref,
    cause: _HardCloseCause)
  =>
    conn._hard_close_tls_upgrading(cause)

  fun ref start_tls(conn: TCPConnection[TCP] ref,
    ssl_ctx: SSLContext val,
    host: String)
    : (None | StartTLSError)
  =>
    StartTLSAlreadyTLS

  fun ref read_again(conn: TCPConnection[TCP] ref) =>
    conn._do_read_again()

  fun ref ssl_handshake_complete(conn: TCPConnection[TCP] ref,
    s: EitherLifecycleEventReceiver[TCP] ref)
  =>
    // TLS upgrade handshake complete — no timer arm needed (timer is
    // already running from the plaintext phase).
    conn._set_state(_Open[TCP])
    s._on_tls_ready()

  fun ref keepalive(conn: TCPConnection[TCP] ref, secs: U32) =>
    conn._do_keepalive(secs)

  fun getsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    conn._do_getsockopt(level, option_name, option_max_size)

  fun getsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    conn._do_getsockopt_u32(level, option_name)

  fun setsockopt(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    conn._do_setsockopt(level, option_name, option)

  fun setsockopt_u32(conn: TCPConnection[TCP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    conn._do_setsockopt_u32(level, option_name, option)

  fun ref idle_timeout(conn: TCPConnection[TCP] ref,
    duration: (IdleTimeout | None))
  =>
    conn._do_idle_timeout(duration)

  fun ref fire_idle_timeout(conn: TCPConnection[TCP] ref) =>
    conn._dispatch_idle_timeout()
    conn._rearm_idle_timer_if_configured()

  fun ref set_timer(conn: TCPConnection[TCP] ref,
    duration: TimerDuration): (TimerToken | SetTimerError)
  =>
    conn._do_set_timer(duration)

  fun is_closed(): Bool => false
  fun sends_allowed(): Bool => false

  fun ref receive(conn: TCPConnection[TCP] ref,
    event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    conn._tcp_ops().receive(event, buffer, size)
