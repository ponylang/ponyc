use "collections"
use "net"

actor Client
  """
  Manages a persistent and possibly pipelined TCP connection to an HTTP server.
  """
  let _host: String
  let _service: String
  var _pipeline: Bool
  var _unsent: List[Request] = List[Request]
  var _sent: List[(Request, Bool)] = List[(Request, Bool)]
  var _conn: (TCPConnection | None) = None

  new create(host: String, service: String, pipeline: Bool = true) =>
    """
    Create a client for the given host and service.
    """
    _host = host
    _service = service
    _pipeline = pipeline

    // TODO: use a timer, detect no data from server after N seconds?

  be apply(request: Request) =>
    """
    Schedule a request.
    """
    _unsent.push(request)
    _send()

  be cancel(request: Request) =>
    """
    Cancel a request.
    """
    try
      for node in _unsent.nodes() do
        if node() is request then
          node.remove()
          request._response(Response)
          return
        end
      end

      for node in _sent.nodes() do
        if node()._1 is request then
          node() = (request, false)
          request._response(Response)
          break
        end
      end
    end

  be _response(response: Response) =>
    """
    Call the request's handler and supply the response.
    """
    try
      (let request, let pending) = _sent.shift()

      if pending then
        request._response(response)
      end

      _send()
    end

  be _connected() =>
    """
    The connection to the server has been established. Send pending requests.
    """
    _send()

  be _connect_failed() =>
    """
    The connection couldn't be established. Cancel all pending requests.
    """
    try
      while true do
        _unsent.pop()._response(Response)
      end
    end

    try
      while true do
        _sent.pop()._1._response(Response)
      end
    end

    _conn = None

  be _closed() =>
    """
    The connection to the server has closed. Reschedule sent requests that
    haven't been cancelled.
    """
    try
      for node in _sent.rnodes() do
        node.remove()

        if node()._2 then
          _unsent.unshift(node()._1)
        end
      end
    end

    _conn = None
    _send()

  fun ref _send() =>
    """
    Send pending requests. If the connection is closed, open it. If we have
    nothing to send and we aren't waiting on any responses, close the
    connection.
    """
    if _unsent.size() == 0 then
      if _sent.size() == 0 then
        try
          (_conn as TCPConnection).dispose()
          _conn = None
        end
      end
      return
    end

    if (_sent.size() > 0) and not _pipeline then
      return
    end

    try
      let conn = _conn as TCPConnection

      try
        repeat
          let request = _unsent.shift()
          request._write(_host, _service, conn)
          _sent.push((request, true))
        until not _pipeline end
      end
    else
      _conn = TCPConnection(_ResponseBuilder(this), _host, _service)
    end
