use "buffered"
use "net"

class _ClientConnHandler is TCPConnectionNotify
  """
  This is the network notification handler for the client.  It passes
  received data to the `HTTPParser` to assemble response `Payload` objects.
  """
  let _session: _ClientConnection
  let _buffer: Reader = Reader
  let _parser: _HTTPParser
  var _delivered: Bool = false

  new iso create(client: _ClientConnection) =>
    """
    The response builder needs to know which Session to forward
    parsed information to.
    """
    _session = client
    _parser = _HTTPParser.response(_session)

  fun ref connected(conn: TCPConnection ref) =>
    """
    Tell the client we have connected.
    """
    _session._connected(conn)

  fun ref connect_failed(conn: TCPConnection ref) =>
    """
    The connection could not be established. Tell the client not to proceed.
    """
    _session._connect_failed(conn)

  fun ref auth_failed(conn: TCPConnection ref) =>
    """
    SSL authentication failed. Tell the client not to proceed.
    """
    _session._auth_failed(conn)

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso): Bool =>
   """
   Pass a received chunk of data to the `_HTTPParser`.
   """
   // TODO: inactivity timer
    _buffer.append(consume data)

    // Let the parser take a look at what has been received.
    try
      _parser.parse(_buffer)
    else
      // Any syntax errors will terminate the connection.
      conn.close()
    end
    true

  fun ref closed(conn: TCPConnection ref) =>
    """
    The connection has closed, possibly prematurely.
    """
    _parser.closed(_buffer)
    _buffer.clear()
    _session._closed(conn)

  fun ref throttled(conn: TCPConnection ref) =>
    """
    TCP connection wants us to stop sending.  We do not do anything with
    this here;  just pass it on to the `HTTPSession`.
    """
    _session.throttled()

  fun ref unthrottle(conn: TCPConnection ref) =>
    """
    TCP can accept more data now.  We just pass this on to the
    `HTTPSession`
    """
    _session.unthrottled()

