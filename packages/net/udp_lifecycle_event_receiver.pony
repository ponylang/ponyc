trait UDPLifecycleEventReceiver[UDP: UDPBackend ref = UDPRuntimeBackend]
  """
  Application-level callbacks for a UDP socket. One receiver per socket, no
  chaining.
  """
  fun ref _socket(): UDPSocket[UDP]

  fun ref _on_bound() =>
    """
    Called when the socket is bound and ready for I/O.
    """
    None

  fun ref _on_bind_failure()
    """
    Called when the socket could not bind. There is no default: ignoring
    bind failure is never correct.
    """

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    """
    Called each time a datagram arrives.

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
