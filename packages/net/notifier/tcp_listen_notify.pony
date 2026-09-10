trait TCPListenNotify
  """
  Callbacks for a TCP listener. Implement this trait and pass it to
  `TCPListener` to handle listener events.

  `on_connected` and `on_not_listening` have no default implementations.
  `on_connected` is where you create the notifier for each accepted
  connection. `on_not_listening` must be implemented because ignoring a listen
  failure silently is never correct.
  """
  fun ref on_listening(listen: TCPListener ref) =>
    """
    Called when the listener is ready to accept connections.
    """
    None

  fun ref on_not_listening(listen: TCPListener ref)
    """
    Called when the listener could not open its socket.
    """

  fun ref on_connected(listen: TCPListener ref):
    ServerTCPConnectionNotify iso^
  ?
    """
    Called when a client connects. Return a `ServerTCPConnectionNotify` for
    the new connection. Raise an error to reject the connection.
    """

  fun ref on_closed(listen: TCPListener ref) =>
    """
    Called after the listener is closed.
    """
    None
