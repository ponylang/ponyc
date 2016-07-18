interface TCPListenNotify
  """
  Notifications for TCP listeners.

  For an example of using this class, please see the documentation for the
  `TCPListener` actor.
  """
  fun ref listening(listen: TCPListener ref) =>
    """
    Called when the listener has been bound to an address.
    """
    None

  fun ref not_listening(listen: TCPListener ref) =>
    """
    Called if it wasn't possible to bind the listener to an address.
    """
    None

  fun ref closed(listen: TCPListener ref) =>
    """
    Called when the listener is closed.
    """
    None

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ ?
    """
    Create a new TCPConnectionNotify to attach to a new TCPConnection for a
    newly established connection to the server.
    """
