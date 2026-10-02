trait ConnectedUDPLifecycleEventReceiver[
  UDP: ConnectedUDPBackend ref = ConnectedUDPRuntimeBackend]
  """
  Application-level callbacks for a connected UDP socket. One receiver per
  socket, no chaining.
  """
  fun ref _socket(): ConnectedUDPSocket[UDP]

  fun ref _on_connected() =>
    """
    Called when the socket is bound and connected to its peer, ready for I/O.
    """
    None

  fun ref _on_bind_failure()
    """
    Called when the socket could not bind. There is no default: ignoring
    bind failure is never correct.
    """

  fun ref _on_connect_failure()
    """
    Called when the socket bound but could not connect to the peer. There is
    no default: ignoring connect failure is never correct.
    """

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    """
    Called each time a datagram arrives from the connected peer.

    Return `KeepReading` to let the read loop take the next datagram, or
    `YieldReading` to stop after this one and give other actors a turn.

    The default returns `KeepReading`. Send-only sockets use this default
    and never override it. Datagrams that arrive on a socket that does not
    override this method are silently discarded.
    """
    KeepReading

  fun ref _on_closed() =>
    """
    Called when the socket is closed.
    """
    None
