use net = ".."

trait UDPSocketNotify
  """
  Callbacks for a UDP socket. Implement this trait and pass it to `UDPSocket`
  to handle socket events.

  `on_bind_failure` has no default implementation because ignoring a bind
  failure silently is never correct.
  """
  fun ref on_bound(sock: UDPSocket ref) =>
    """
    Called when the socket is bound and ready for I/O.
    """
    None

  fun ref on_bind_failure(sock: UDPSocket ref)
    """
    Called when the socket could not bind.
    """

  fun ref on_received(sock: UDPSocket ref,
    data: Array[U8] iso,
    from: net.NetAddress val)
    : net.ReadAction
  =>
    """
    Called each time a datagram arrives.

    Return `KeepReading` to let the read loop take the next datagram, or
    `YieldReading` to stop after this one and give other actors a turn.

    The default returns `KeepReading`. Send-only sockets use this default
    and never override it. Datagrams that arrive on a socket that does not
    override this method are silently discarded.
    """
    net.KeepReading

  fun ref on_closed(sock: UDPSocket ref) =>
    """
    Called when the socket is closed.
    """
    None
