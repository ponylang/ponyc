use net = ".."

trait ConnectedUDPSocketNotify
  """
  Callbacks for a connected UDP socket. Implement this trait and pass it to
  `ConnectedUDPSocket` to handle socket events.

  `on_bind_failure` and `on_connect_failure` have no default implementation
  because ignoring either failure silently is never correct.
  """
  fun ref on_connected(sock: ConnectedUDPSocket ref) =>
    """
    Called when the socket is bound and connected, ready for I/O.
    """
    None

  fun ref on_bind_failure(sock: ConnectedUDPSocket ref)
    """
    Called when the socket could not bind.
    """

  fun ref on_connect_failure(sock: ConnectedUDPSocket ref)
    """
    Called when the socket bound but could not connect to the peer.
    """

  fun ref on_received(sock: ConnectedUDPSocket ref,
    data: Array[U8] iso)
    : net.ReadAction
  =>
    """
    Called each time a datagram arrives from the connected peer.

    Return `KeepReading` to let the read loop take the next datagram, or
    `YieldReading` to stop after this one and give other actors a turn.

    The default returns `KeepReading`. Send-only sockets use this default
    and never override it. Datagrams that arrive on a socket that does not
    override this method are silently discarded.
    """
    net.KeepReading

  fun ref on_closed(sock: ConnectedUDPSocket ref) =>
    """
    Called when the socket is closed.
    """
    None
