interface TCPConnectionNotify
  """
  Notifications for TCP connections.
  """
  fun ref accepted(conn: TCPConnection ref) =>
    """
    Called when a TCPConnection is accepted by a TCPListener.
    """
    None

  fun ref connecting(conn: TCPConnection ref) =>
    """
    Called if name resolution succeeded for a TCPConnection and we are now
    waiting for a connection to the server to succeed.
    """
    None

  fun ref connected(conn: TCPConnection ref) =>
    """
    Called when we have successfully connected to the server.
    """
    None

  fun ref connect_failed(conn: TCPConnection ref) =>
    """
    Called when we have failed to connect to all possible addresses for the
    server. At this point, the connection will never be established.
    """
    None

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Called when new data is received on the connection.
    """
    None

  fun ref closed(conn: TCPConnection ref) =>
    """
    Called when the connection is closed.
    """
    None

interface TCPListenNotify
  """
  Notifications for TCP listeners.
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

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^
    """
    Create a new TCPConnectionNotify to attach to a new TCPConnection for a
    newly established connection to the server.
    """
