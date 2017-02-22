use "net"
use "collections"

actor _ServerConnection is HTTPSession
  """
  Manages a stream of requests coming into a server from a single client,
  dispatches those request to a back-end, and returns the responses back
  to the client.
  """
  let _backend: HTTPHandler
  let _logger: Logger
  let _conn: TCPConnection
  let _client_ip: String
  let _pending: List[Payload val] = _pending.create()
  var _active_request: (Payload val | None) = None
  var _active_response: (Payload val | None) = None
  var _keepalive: Bool = true
  var _body_bytes_sent: USize = 0

  new create(handlermaker: HandlerFactory val, logger: Logger,
    conn: TCPConnection, client_ip: String)
  =>
    """
    Create a connection actor to manage communication with to a new
    client.  We also create an instance of the application's back-end
    handler that will process incoming requests.
    """
    _backend = handlermaker(this)
    _logger = logger
    _conn = conn
    _client_ip = client_ip

  be _deliver(request: Payload val) =>
    """
    Dispatch requests. At the time this behavior is called for StreamTransfer
    and ChunkTransfer encodings, only the headers of the request may have
    been received. Receiving and dealing with the body, which could be
    quite large in POST methods, is up to the chosen application handler.

    The client can send several requests without waiting for a response,
    but they are only passed to the back end one at a time.  Only when all
    of the response to a processed request has been sent is the next request
    processed.
    """
    let frozen_request = recover val consume request end

    if _active_request is None
    then
        // Backend is not busy, so pass this request on for processing.
        _active_request = frozen_request
        _keepalive = try
          frozen_request("Connection") != "close"
        else
          frozen_request.proto != "HTTP/1.0"
        end
        _body_bytes_sent = 0
        _backend(frozen_request)
    else
        _pending.push(frozen_request)
        // Backpressure incoming requests if the queue grows too much.
        // The backpressure prevents filling up memory with queued
        // requests in the case of a runaway client.
        if _pending.size() > 2 then _conn.mute() end
    end
 
  be _chunk(data: ByteSeq val) =>
    """
    Receive some `request` body data, which we pass on to the handler.
    """
    _body_bytes_sent = _body_bytes_sent + data.size()
    _backend.chunk(consume data)
      
  be _finish() =>
    """
    Inidcates that the last *inbound* body chunk has been sent to
    `_chunk`.  This is passed on to the back end.
    """
    _backend.finished()

  be cancel(msg: Payload val) =>
    """
    Cancel the current response.  The connection has closed.
    """
    match _active_response
      | let p: Payload val =>
          finish()
          _backend.cancelled()
    end

  be _closed() =>
    _backend.finished()
    _conn.unmute()

  be apply(response: Payload val) =>
    """
    Initiate transmission of the HTTP Response message for the current
    Request.
    """
    _conn.unmute()
    _active_response = response
    _send(response)

    // Clear all pending after an error response.
    if (response.status == 0) or
      (response.status >= 300)
    then
      _pending.clear()
      end

  fun ref _dispatch_pending() =>
    """
    If we have pending requests, dispatch the next one.
    """
    try
      let request = _pending.pop()
      _active_request = request
      _backend(consume request)
    else
      _active_request = None
    end

  fun ref _send(response: Payload val) =>
    """
    Send a single response.
    """
    let okstatus = (response.status < 300)
    response._write(_keepalive and okstatus, _conn)

    if response.has_body()
    then
      match response.transfer_mode
        | OneshotTransfer => finish() // Already sent
      else
        _backend.need_body()  // To be sent later
      end
    else
      finish()
    end

  be write(data: ByteSeq val) =>
    """
    Write low level outbound raw byte stream.
    """
    _body_bytes_sent = _body_bytes_sent + data.size()
    _conn.write(data)

  be finish() =>
    """
    We are done sending a response.  We can close the connection if
    `keepalive` was not requested.
    """
    try
      _logger(_client_ip, _body_bytes_sent,
         (_active_request as Payload val),
         (_active_response as Payload val))
    end

    _active_request = None
    _active_response = None
    
    if not _keepalive then
      _conn.dispose()
      _pending.clear()
    end

  be throttled() =>
    """
    TCP connection can not accept data for a while.
    """
    _backend.throttled()

  be unthrottled() =>
    """
    TCP connection can not accept data for a while.
    """
    _backend.unthrottled()

  be _mute() =>
    _conn.mute()

  be _unmute() =>
    _conn.unmute()
    
