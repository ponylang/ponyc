use "net"

class _RequestBuilder is TCPConnectionNotify
  """
  This builds a request payload using received chunks of data.
  """
  let _handler: RequestHandler
  var _server: (_ServerConnection | None) = None
  let _buffer: Buffer = Buffer
  let _builder: _PayloadBuilder = _PayloadBuilder.request()

  new iso create(handler: RequestHandler) =>
    """
    The request builder needs to know how to handle requests.
    """
    _handler = handler

  fun ref accepted(conn: TCPConnection ref) =>
    """
    Create a server connection to handle response ordering.
    """
    _server = _ServerConnection(_handler, conn)

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    """
    Assemble chunks of data into a request. When we have a whole request,
    dispatch it.
    """
    // TODO: inactivity timer
    // add a "reset" API to Timers
    _buffer.append(consume data)
    _builder.parse(_buffer)

    match _builder.state()
    | _PayloadReady
    | _PayloadError =>
      try (_server as _ServerConnection).dispatch(_builder.done()) end
    end
