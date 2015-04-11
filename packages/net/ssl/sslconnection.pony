use "collections"
use "net"

class SSLConnection is TCPConnectionNotify
  """
  Wrap another protocol in an SSL connection.
  """
  let _notify: TCPConnectionNotify
  let _ssl: SSL
  var _connected: Bool = false
  let _pending: List[Bytes] = _pending.create()

  new create(notify: TCPConnectionNotify, ssl: SSL) =>
    """
    Initialise with a wrapped protocol and an SSL session.
    """
    _notify = notify
    _ssl = ssl

  fun ref accepted(conn: TCPConnection ref) =>
    """
    Forward to the wrapped protocol.
    """
    _notify.accepted(conn)

  fun ref connecting(conn: TCPConnection ref, count: U64) =>
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

  fun ref sent(conn: TCPConnection ref, data: Bytes): Bytes ? =>
    """
    Pass the data to the SSL session and check for both new application data
    and new destination data.
    """
    if _connected then
      _ssl.write(data)
    else
      _pending.push(data)
    end
    _poll(conn)
    error

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Pass the data to the SSL session and check for both new application data
    and new destination data.
    """
    _ssl.receive(consume data)
    _poll(conn)

  fun ref closed(conn: TCPConnection ref) =>
    """
    Forward to the wrapped protocol.
    """
    _ssl.dispose()
    _connected = false
    _pending.clear()
    _notify.closed(conn)

  fun ref _poll(conn: TCPConnection ref) =>
    """
    Checks for both new application data and new destination data. Informs the
    wrapped protocol that is has connected when the handshake is complete.
    """
    if not _connected then
      if _ssl.ready() then
        _connected = true
        _notify.connected(conn)

        try
          while _pending.size() > 0 do
            _ssl.write(_pending.shift())
          end
        end
      end
    end

    try
      _notify.received(conn, _ssl.read())
    end

    try
      conn.write_final(_ssl.send())
    end
