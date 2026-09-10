trait ref UDPBackend
  """
  The UDP operations a socket needs from the runtime. `UDPRuntimeBackend` is
  the production implementation; test code can substitute a fake to drive the
  socket state machine without real sockets.
  """
  new create()

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
    """
    Bind a UDP socket to `host`:`port` and subscribe `the_actor` for
    readiness events. Returns the ASIO event, or a null event on failure.
    """

  fun ref close(fd: U32)
    """
    Close the socket.
    """

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
    """
    Receive one datagram into `buffer`. Returns the tri-state socket result,
    the number of bytes received, and the sender address.
    """

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
    """
    Send one datagram. Returns `SocketResultOk` on success,
    `SocketResultRetry` when the OS buffer is full, or `SocketResultError`
    on failure. No byte count: POSIX datagram `sendto` is all-or-nothing.
    """

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool
    """
    Fill `ip` with the local address of `fd`. Returns true on success.
    """
