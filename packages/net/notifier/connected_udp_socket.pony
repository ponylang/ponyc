use net = ".."

actor ConnectedUDPSocket
  is (net.ConnectedUDPSocketActor & net.ConnectedUDPLifecycleEventReceiver)
  """
  A connected UDP socket actor driven by a
  [`ConnectedUDPSocketNotify`](/net/notifier-ConnectedUDPSocketNotify/).

  Wraps the net package's `ConnectedUDPSocket` class,
  `ConnectedUDPSocketActor` trait, and
  `ConnectedUDPLifecycleEventReceiver` trait into a single actor.

  `send` is synchronous and returns a `UDPSendResult` — use it from within
  callbacks where the result matters. `write` is a fire-and-forget
  behavior for sending from other actors; the result is discarded.
  """
  var _udp: net.ConnectedUDPSocket = net.ConnectedUDPSocket.none()
  var _notify: ConnectedUDPSocketNotify ref

  new create(auth: net.UDPAuth,
    notify: ConnectedUDPSocketNotify iso,
    host: String,
    port: String,
    peer_host: String,
    peer_port: String,
    read_buffer_size: net.ReadBufferSize = net.DefaultReadBufferSize(),
    ip_version: net.IPVersion = net.DualStack,
    max_datagrams_per_turn: USize = 256)
  =>
    """
    Bind a UDP socket to `host`:`port` and connect it to
    `peer_host`:`peer_port`. Port `"0"` for the local side lets the OS
    assign an ephemeral port.
    """
    _notify = consume notify
    _udp =
      net.ConnectedUDPSocket(
        auth,
        host,
        port,
        peer_host,
        peer_port,
        this,
        this,
        read_buffer_size,
        ip_version,
        max_datagrams_per_turn)

  fun ref _socket(): net.ConnectedUDPSocket =>
    _udp

  be write(data: ByteSeq) =>
    """
    Send a datagram to the connected peer. Fire-and-forget: the send is
    silently dropped if the socket is not open. Use `send` from within a
    callback when the result matters.
    """
    _udp.send(data)

  fun ref send(data: ByteSeq): net.UDPSendResult =>
    """
    Send one datagram to the connected peer. Returns `UDPSendOk` when the
    datagram was handed to the OS. UDP sends are synchronous and
    all-or-nothing.

    Callable from any callback (where `sock` is `ref`). From outside the
    actor, use the `write` behavior instead.
    """
    _udp.send(data)

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

  fun ref remote_address(): net.NetAddress =>
    """
    Return the remote peer address the socket is connected to.
    """
    _udp.remote_address()

  fun is_open(): Bool =>
    """
    True when the socket is connected and has not been closed.
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

  fun ref _on_connected() =>
    _notify.on_connected(this)

  fun ref _on_bind_failure() =>
    _notify.on_bind_failure(this)

  fun ref _on_connect_failure() =>
    _notify.on_connect_failure(this)

  fun ref _on_received(data: Array[U8] iso): net.ReadAction =>
    _notify.on_received(this, consume data)

  fun ref _on_closed() =>
    _notify.on_closed(this)
