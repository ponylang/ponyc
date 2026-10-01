trait ref ConnectedUDPBackend
  """
  The operations a connected UDP socket needs from the runtime. Overlaps with
  `UDPBackend` for bind and receive; adds connect and send for connected-mode
  I/O. `ConnectedUDPRuntimeBackend` is the production implementation. Test
  code can substitute a fake that implements only this trait.
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

  fun ref connect(fd: U32,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : Bool
    """
    Set the default peer address on an already-bound UDP socket. Returns
    true on success.
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

  fun ref send(fd: U32,
    data: ByteSeq)
    : SocketResult
    """
    Send one datagram to the connected peer. Returns `SocketResultOk` on
    success, `SocketResultRetry` when the OS buffer is full, or
    `SocketResultError` on failure.
    """

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool
    """
    Fill `ip` with the local address of `fd`. Returns true on success.
    """

  fun ref peername(fd: U32, ip: NetAddress tag): Bool
    """
    Fill `ip` with the remote peer address of `fd`. Returns true on success.
    """
