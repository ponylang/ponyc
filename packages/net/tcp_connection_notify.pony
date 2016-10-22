interface TCPConnectionNotify
  """
  Notifications for TCP connections.

  For an example of using this class please see the documentation for the
  `TCPConnection` and `TCPListener` actors.
  """
  fun ref accepted(conn: TCPConnection ref) =>
    """
    Called when a TCPConnection is accepted by a TCPListener.
    """
    None

  fun ref connecting(conn: TCPConnection ref, count: U32) =>
    """
    Called if name resolution succeeded for a TCPConnection and we are now
    waiting for a connection to the server to succeed. The count is the number
    of connections we're trying. The notifier will be informed each time the
    count changes, until a connection is made or connect_failed() is called.
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

  fun ref auth_failed(conn: TCPConnection ref) =>
    """
    A raw TCPConnection has no authentication mechanism. However, when
    protocols are wrapped in other protocols, this can be used to report an
    authentication failure in a lower level protocol (e.g. SSL).
    """
    None

  fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq =>
    """
    Called when data is sent on the connection. This gives the notifier an
    opportunity to modify sent data before it is written. To swallow data,
    return an empty string.
    """
    data

  fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter =>
    """
    Called when multiple chunks of data are sent to the connection in a single
    call. This gives the notifier an opportunity to modify the sent data chunks
    before they are written. To swallow the send, return an empty
    Array[String].
    """
    data

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso): Bool =>
    """
    Called when new data is received on the connection. Return true if you
    want to continue receiving messages without yielding until you read
    max_size on the TCPConnection. Return false to cause the TCPConnection
    to yield now.
    """
    true

  fun ref expect(conn: TCPConnection ref, qty: USize): USize =>
    """
    Called when the connection has been told to expect a certain quantity of
    bytes. This allows nested notifiers to change the expected quantity, which
    allows a lower level protocol to handle any framing (e.g. SSL).
    """
    qty

  fun ref closed(conn: TCPConnection ref) =>
    """
    Called when the connection is closed.
    """
    None

  fun ref throttled(conn: TCPConnection ref) =>
    """
    Called when the connection starts experiencing TCP backpressure. You should
    respond to this by pausing additional calls to `write` and `writev` until
    you are informed that pressure has been released. Failure to respond to
    the `throttled` notification will result in outgoing data queuing in the
    connection and increasing memory usage.
    """
    None

  fun ref unthrottled(conn: TCPConnection ref) =>
    """
    Called when the connection stops experiencing TCP backpressure. Upon
    receiving this notification, you should feel free to start making calls to
    `write` and `writev` again.
    """
    None

