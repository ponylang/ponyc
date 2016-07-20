use "collections"
use "net"

class SSLConnection is TCPConnectionNotify
  """
  Wrap another protocol in an SSL connection.
  """
  let _notify: TCPConnectionNotify
  let _ssl: SSL
  var _connected: Bool = false
  var _expect: USize = 0
  var _closed: Bool = false
  let _pending: List[ByteSeq] = _pending.create()

  new iso create(notify: TCPConnectionNotify iso, ssl: SSL iso) =>
    """
    Initialise with a wrapped protocol and an SSL session.
    """
    _notify = consume notify
    _ssl = consume ssl

  fun ref accepted(conn: TCPConnection ref) =>
    """
    Forward to the wrapped protocol.
    """
    _notify.accepted(conn)

  fun ref connecting(conn: TCPConnection ref, count: U32) =>
    """
    Forward to the wrapped protocol.
    """
    _notify.connecting(conn, count)

  fun ref connected(conn: TCPConnection ref) =>
    """
    Swallow this event until the handshake is complete.
    """
    _poll(conn)

  fun ref connect_failed(conn: TCPConnection ref) =>
    """
    Forward to the wrapped protocol.
    """
    _notify.connect_failed(conn)

  fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq =>
    """
    Pass the data to the SSL session and check for both new application data
    and new destination data.
    """
    if _connected then
      try
        _ssl.write(data)
      end
    else
      _pending.push(data)
    end

    _poll(conn)
    ""

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso): Bool =>
    """
    Pass the data to the SSL session and check for both new application data
    and new destination data.
    """
    _ssl.receive(consume data)
    _poll(conn)
    true

  fun ref expect(conn: TCPConnection ref, qty: USize): USize =>
    """
    Keep track of the expect count for the wrapped protocol. Always tell the
    TCPConnection to read all available data.
    """
    _expect = _notify.expect(conn, qty)
    0

  fun ref closed(conn: TCPConnection ref) =>
    """
    Forward to the wrapped protocol.
    """
    _closed = true

    _poll(conn)
    _ssl.dispose()

    _connected = false
    _pending.clear()
    _notify.closed(conn)

  fun ref _poll(conn: TCPConnection ref) =>
    """
    Checks for both new application data and new destination data. Informs the
    wrapped protocol that is has connected when the handshake is complete.
    """
    match _ssl.state()
    | SSLReady =>
      if not _connected then
        _connected = true
        _notify.connected(conn)

        try
          while _pending.size() > 0 do
            _ssl.write(_pending.shift())
          end
        end
      end
    | SSLAuthFail =>
      _notify.auth_failed(conn)

      if not _closed then
        conn.close()
      end

      return
    | SSLError =>
      if not _closed then
        conn.close()
      end

      return
    end

    try
      while true do
        let r = _ssl.read(_expect)

        if r isnt None then
          _notify.received(conn, (consume r) as Array[U8] iso^)
        else
          break
        end
      end
    end

    try
      while _ssl.can_send() do
        conn.write_final(_ssl.send())
      end
    end
