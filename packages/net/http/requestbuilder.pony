use "net"

class _RequestBuilder is TCPConnectionNotify
  """
  This builds a request payload using received chunks of data.
  """
  let _handler: RequestHandler
  let _logger: Logger
  var _server: (_ServerConnection | None) = None
  let _buffer: Buffer = Buffer
  let _builder: _PayloadBuilder = _PayloadBuilder.request()

  new iso create(handler: RequestHandler, logger: Logger) =>
    """
    The request builder needs to know how to handle requests.
    """
    _handler = handler
    _logger = logger

  fun ref accepted(conn: TCPConnection ref) =>
    """
    Create a server connection to handle response ordering.
    """
    let host = try _logger.client_ip(conn.remote_address()) else "-" end
    _server = _ServerConnection(_handler, _logger, conn, host)

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Assemble chunks of data into a request. When we have a whole request,
    dispatch it.
    """
    // TODO: inactivity timer
    // add a "reset" API to Timers
    _buffer.append(consume data)

    while true do
      _builder.parse(_buffer)

      match _builder.state()
      | _PayloadReady =>
        try (_server as _ServerConnection).dispatch(_builder.done()) end
      | _PayloadError =>
        conn.close()
        break
      else
        break
      end
    end
