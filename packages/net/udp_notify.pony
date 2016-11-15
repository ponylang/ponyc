interface UDPNotify
  """
  Notifications for UDP connections.

  For an example of using this class please see the documentatoin for the
  `UDPSocket` actor.
  """
  fun ref listening(sock: UDPSocket ref) =>
    """
    Called when the socket has been bound to an address.
    """
    None

  fun ref not_listening(sock: UDPSocket ref)
    """
    Called if it wasn't possible to bind the socket to an address.

    It is expected to implement proper error handling. You need to opt in to
    ignoring errors, which can be implemented like this:

    ```
    fun ref connect_failed(conn: TCPConnection ref) =>
      None
    ```
    """

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    """
    Called when new data is received on the socket.
    """
    None

  fun ref closed(sock: UDPSocket ref) =>
    """
    Called when the socket is closed.
    """
    None
