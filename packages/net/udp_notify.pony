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

    ```pony
    fun ref not_listening(sock: UDPSocket ref) =>
      None
    ```
    """

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    """
    Called when a datagram is received on the socket.

    `data` is truncated to the socket's read buffer size -- the `size` argument
    to the `UDPSocket` listen constructors, default 1024. A datagram larger
    than that buffer is delivered as its first `size` bytes; the remainder is
    silently discarded. This is true on every platform, and there is currently
    no way to tell from this callback that truncation happened or how many
    bytes were lost. To receive whole datagrams, size the read buffer to your
    protocol's largest expected datagram (the UDP maximum is 65507 bytes for
    IPv4).

    A zero-length datagram is valid and is delivered here with an empty `data`.
    """
    None

  fun ref closed(sock: UDPSocket ref) =>
    """
    Called when the socket is closed.
    """
    None
