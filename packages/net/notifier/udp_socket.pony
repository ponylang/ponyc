use net = ".."

actor UDPSocket is (net.UDPSocketActor & net.UDPLifecycleEventReceiver)
  """
  A UDP socket actor driven by a
  [`UDPSocketNotify`](/net/notifier-UDPSocketNotify/).

  Wraps the net package's `UDPSocket` class, `UDPSocketActor` trait, and
  `UDPLifecycleEventReceiver` trait into a single actor.

  `send_to` is synchronous and returns a `SendToResult` — use it from within
  callbacks where the result matters. `write_to` is a fire-and-forget
  behavior for sending from other actors; the result is discarded.
  """
  var _udp: net.UDPSocket = net.UDPSocket.none()
  var _notify: UDPSocketNotify ref

  new create(auth: net.UDPAuth,
    notify: UDPSocketNotify iso,
    host: String,
    port: String,
    read_buffer_size: net.ReadBufferSize = net.DefaultReadBufferSize(),
    ip_version: net.IPVersion = net.DualStack,
    max_datagrams_per_turn: USize = 256)
  =>
    """
    Bind a UDP socket to `host`:`port`. Port `"0"` lets the OS assign an
    ephemeral port.
    """
    _notify = consume notify
    _udp =
      net.UDPSocket(
        auth, host, port, this, this,
        read_buffer_size, ip_version, max_datagrams_per_turn)

  // --- UDPSocketActor --------------------------------------------------------
  fun ref _socket(): net.UDPSocket =>
    _udp

  // --- Behaviors -------------------------------------------------------------
  be write_to(data: ByteSeq, to: net.NetAddress val) =>
    """
    Send a datagram. Fire-and-forget: the send is silently dropped if the
    socket is not open. Use `send_to` from within a callback when the result
    matters.
    """
    _udp.send_to(data, to)

  // --- Synchronous methods ---------------------------------------------------
  fun ref send_to(data: ByteSeq, to: net.NetAddress box): net.SendToResult =>
    """
    Send one datagram to `to`. Returns `SendToOk` when the datagram was
    handed to the OS. UDP sends are synchronous and all-or-nothing.

    Callable from any callback (where `sock` is `ref`). From outside the
    actor, use the `write_to` behavior instead.
    """
    _udp.send_to(data, to)

  fun ref close() =>
    """
    Close the socket. No graceful shutdown: the fd is closed immediately.
    """
    _udp.close()

  fun ref local_address(): net.NetAddress =>
    """
    Return the local IP address the socket is bound to.
    """
    _udp.local_address()

  fun is_open(): Bool =>
    """
    True when the socket is bound and has not been closed.
    """
    _udp.is_open()

  fun is_closed(): Bool =>
    """
    True when the socket has been closed.
    """
    _udp.is_closed()

  fun get_so_rcvbuf(): (U32, U32) =>
    """
    Get the OS receive buffer size. Returns (errno, value).
    """
    _udp.get_so_rcvbuf()

  fun set_so_rcvbuf(bufsize: U32): U32 =>
    """
    Set the OS receive buffer size. Returns 0 on success, or errno.
    """
    _udp.set_so_rcvbuf(bufsize)

  fun get_so_sndbuf(): (U32, U32) =>
    """
    Get the OS send buffer size. Returns (errno, value).
    """
    _udp.get_so_sndbuf()

  fun set_so_sndbuf(bufsize: U32): U32 =>
    """
    Set the OS send buffer size. Returns 0 on success, or errno.
    """
    _udp.set_so_sndbuf(bufsize)

  fun getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    """
    Get a socket option as a U32. Returns (errno, value).
    """
    _udp.getsockopt_u32(level, option_name)

  fun setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    """
    Set a socket option as a U32. Returns 0 on success, or errno.
    """
    _udp.setsockopt_u32(level, option_name, option)

  // --- UDPLifecycleEventReceiver ---------------------------------------------
  fun ref _on_bound() =>
    _notify.on_bound(this)

  fun ref _on_bind_failure() =>
    _notify.on_bind_failure(this)

  fun ref _on_received(data: Array[U8] iso, from: net.NetAddress val)
    : net.ReadAction
  =>
    _notify.on_received(this, consume data, from)

  fun ref _on_closed() =>
    _notify.on_closed(this)
