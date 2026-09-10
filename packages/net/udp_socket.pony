class UDPSocket[UDP: UDPBackend ref = UDPRuntimeBackend]
  """
  A UDP socket: bind, send datagrams, receive datagrams, and close. A
  `UDPSocketActor` owns one and delegates to it.

  Create with `UDPSocket(auth, host, port, enclosing, ler)`, using
  `UDPSocket.none()` as the field initializer before that. See the package
  documentation for the full lifecycle.
  """
  var _udp: UDP = UDP
  var _state: _UDPSocketState[UDP] ref = _UDPNone[UDP]
  var _event: AsioEventID = AsioEvent.none()
  var _fd: U32 = -1
  let _host: String
  let _port: String
  let _ip_version: IPVersion
  let _read_buffer_size: USize
  let _max_datagrams_per_turn: USize
  var _enclosing: (UDPSocketActor[UDP] ref | None)
  var _ler: (UDPLifecycleEventReceiver[UDP] ref | None)

  new create(auth: UDPAuth,
    host: String,
    port: String,
    enclosing: UDPSocketActor[UDP] ref,
    ler: UDPLifecycleEventReceiver[UDP] ref,
    read_buffer_size: ReadBufferSize = DefaultReadBufferSize(),
    ip_version: IPVersion = DualStack,
    max_datagrams_per_turn: USize = 256)
  =>
    """
    Bind a UDP socket to `host`:`port`. Port `"0"` lets the OS assign an
    ephemeral port.
    """
    _host = host
    _port = port
    _ip_version = ip_version
    _read_buffer_size = read_buffer_size()
    _max_datagrams_per_turn = max_datagrams_per_turn
    _enclosing = enclosing
    _ler = ler

    enclosing._finish_initialization()

  new none() =>
    """
    Placeholder for field initialization before the real constructor runs.
    """
    _host = ""
    _port = ""
    _ip_version = DualStack
    _read_buffer_size = 16384
    _max_datagrams_per_turn = 256
    _enclosing = None
    _ler = None

  fun ref send_to(data: ByteSeq, to: NetAddress box): SendToResult =>
    """
    Send one datagram to `to`. Returns `SendToOk` when the datagram was
    handed to the OS. UDP sends are synchronous and all-or-nothing: the
    entire datagram goes out or nothing does.
    """
    _state.send_to(this, data, to)

  fun ref close() =>
    """
    Close the socket. No graceful shutdown: the fd is closed immediately.
    """
    _state.close(this)

  fun ref local_address(): NetAddress =>
    """
    Return the local IP address. If the socket is closed the address returned
    is invalid.
    """
    let ip = recover NetAddress end
    _udp.sockname(_fd, ip)
    ip

  fun ref read_again() =>
    """
    Re-enter the read loop. Called internally after yielding or exhausting the
    per-turn budget; applications do not call this directly.
    """
    _state.read_again(this)

  fun is_open(): Bool =>
    """
    True when the socket is bound and has not been closed.
    """
    _state.is_open()

  fun is_closed(): Bool =>
    """
    True when the socket has been closed.
    """
    _state.is_closed()

  fun get_so_rcvbuf(): (U32, U32) =>
    """
    Get the OS receive buffer size for this socket.
    Returns (errno, value). On success errno is 0.
    """
    getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())

  fun set_so_rcvbuf(bufsize: U32): U32 =>
    """
    Set the OS receive buffer size for this socket.
    Returns 0 on success, or a non-zero errno.
    """
    setsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), bufsize)

  fun get_so_sndbuf(): (U32, U32) =>
    """
    Get the OS send buffer size for this socket.
    Returns (errno, value). On success errno is 0.
    """
    getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())

  fun set_so_sndbuf(bufsize: U32): U32 =>
    """
    Set the OS send buffer size for this socket.
    Returns 0 on success, or a non-zero errno.
    """
    setsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf(), bufsize)

  fun getsockopt(level: I32,
    option_name: I32,
    option_max_size: USize = 4)
    : (U32, Array[U8] iso^)
  =>
    """
    General interface to `getsockopt(2)`. Returns `(0, data)` on success or
    `(errno, undefined)` on failure. Returns `(1, empty)` when the socket is
    not open. For commonly-tuned options, prefer `get_so_rcvbuf` and
    `get_so_sndbuf`.
    """
    _state.getsockopt(this, level, option_name, option_max_size)

  fun getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    """
    Wrapper for `getsockopt(2)` where the kernel returns a C `uint32_t`.
    Returns `(0, value)` on success or `(errno, undefined)` on failure.
    Returns `(1, 0)` when the socket is not open.
    """
    _state.getsockopt_u32(this, level, option_name)

  fun setsockopt(level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    """
    General interface to `setsockopt(2)`. The caller is responsible for the
    correct size, byte contents, and byte order of `option`. Returns 0 on
    success, or a non-zero errno. Returns non-zero when the socket is not
    open. For commonly-tuned options, prefer `set_so_rcvbuf` and
    `set_so_sndbuf`.
    """
    _state.setsockopt(this, level, option_name, option)

  fun setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    """
    Wrapper for `setsockopt(2)` where the kernel expects a C `uint32_t`.
    Returns 0 on success, or a non-zero errno. Returns non-zero when the
    socket is not open.
    """
    _state.setsockopt_u32(this, level, option_name, option)

  //
  // Internal methods called by state classes
  //
  fun ref _set_state(state: _UDPSocketState[UDP] ref) =>
    _state = state

  fun ref _dispatch_io_event(flags: U32) =>
    if AsioEvent.errored(flags) then
      _do_close()
      return
    end

    if AsioEvent.readable(flags) then
      _pending_reads()
    end

  fun ref _pending_reads() =>
    match \exhaustive\ _ler
    | let ler: UDPLifecycleEventReceiver[UDP] ref =>
      var total_bytes_read: USize = 0
      var datagrams_read: USize = 0

      while true do
        if _state.is_closed() then
          return
        end

        if total_bytes_read >= _read_buffer_size then
          _queue_read()
          return
        end

        if datagrams_read >= _max_datagrams_per_turn then
          _queue_read()
          return
        end

        let buffer =
          recover Array[U8] .> undefined(_read_buffer_size) end
        (let result, let count, let from) =
          _udp.recvfrom(_event, buffer.cpointer(), _read_buffer_size)

        match \exhaustive\ result
        | SocketResultOk =>
          buffer.truncate(count)
          total_bytes_read = total_bytes_read + count
          datagrams_read = datagrams_read + 1
          match \exhaustive\ ler._on_received(consume buffer, consume from)
          | KeepReading => None
          | YieldReading =>
            _queue_read()
            return
          end
        | SocketResultRetry =>
          return
        | SocketResultError =>
          _queue_read()
          return
        end
      end
    | None =>
      _Unreachable()
    end

  fun ref _do_send_to(data: ByteSeq, to: NetAddress box): SendToResult =>
    match \exhaustive\ _udp.sendto(_fd, data, to)
    | SocketResultOk => SendToOk
    | SocketResultRetry => SendToWouldBlock
    | SocketResultError => SendToError
    end

  fun ref _do_close() =>
    PonyAsio.unsubscribe(_event)
    ifdef not windows then
      _udp.close(_fd)
      _fd = -1
    end
    _state = _UDPClosed[UDP]
    match \exhaustive\ _ler
    | let ler: UDPLifecycleEventReceiver[UDP] ref =>
      ler._on_closed()
    | None =>
      _Unreachable()
    end

  fun ref _do_read_again() =>
    _pending_reads()

  fun ref _queue_read() =>
    match \exhaustive\ _enclosing
    | let e: UDPSocketActor[UDP] ref =>
      e._read_again()
    | None =>
      _Unreachable()
    end

  fun ref _event_notify(event: AsioEventID, flags: U32) =>
    if event isnt _event then
      if AsioEvent.disposable(flags) then
        PonyAsio.destroy(event)
      end
      return
    end

    _state.event_notify(this, flags)

    if AsioEvent.disposable(flags) then
      PonyAsio.destroy(event)
      _event = AsioEvent.none()
    end

  fun ref _finish_initialization() =>
    match _state
    | let _: _UDPClosed[UDP] => return
    end

    match \exhaustive\ (_enclosing, _ler)
    | (let e: UDPSocketActor[UDP] ref,
      let ler: UDPLifecycleEventReceiver[UDP] ref) =>
      _event = _udp.bind(e, _host, _port where ip_version = _ip_version)

      if not _event.is_null() then
        _fd = PonyAsio.event_fd(_event)
        _state = _UDPOpen[UDP]
        ler._on_bound()
      else
        _state = _UDPClosed[UDP]
        ler._on_bind_failure()
      end
    | (_, _) =>
      _Unreachable()
    end

  //
  // Socket option delegates for state classes
  //
  fun _do_getsockopt(level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    _OSSocket.getsockopt(_fd, level, option_name, option_max_size)

  fun _do_getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    _OSSocket.getsockopt_u32(_fd, level, option_name)

  fun _do_setsockopt(level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    _OSSocket.setsockopt(_fd, level, option_name, option)

  fun _do_setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    _OSSocket.setsockopt_u32(_fd, level, option_name, option)
