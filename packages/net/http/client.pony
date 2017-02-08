use "collections"
use "net"
use "net/ssl"

class HTTPClient
  """
  Manages a group of HTTP connections on behalf of a client application.
  A client should create one instance of this class.
  """
  let _auth: TCPConnectionAuth
  let _sslctx: SSLContext
  let _pipeline: Bool
  let _sessions: Map[_HostService, _ClientConnection] = _sessions.create()
 
  new create(auth: TCPConnectionAuth, sslctx: (SSLContext | None) = None,
    pipeline: Bool = true)
  =>
    """
    Create the context in which all HTTP sessions will originate.
    """
    _auth = auth

    _sslctx = try
      sslctx as SSLContext
    else
      recover
        let newssl = SSLContext
        newssl.set_client_verify(false)
        newssl
        end
    end

    _pipeline = pipeline

  fun ref apply(request: Payload trn, handlermaker: HandlerFactory val):
    Payload val ? =>
    """
    Schedule a request on an HTTP session.  If a new connection is created,
    a new instance of the application's Receive Handler will be created
    for it.  A `val` copy of the `Payload` is returned, and it can not be
    modified after this point.
    This is useful in Stream and Chunked transfer modes, so that the
    application can follow up with calls to `Client.send_body`.
    """
    let session = _get_session(request.url, handlermaker)
    let mode = request.transfer_mode
    request.session = session
    let valrequest: Payload val = consume request
    session(valrequest)
    valrequest
    
/*
  fun ref cancel(request: Payload val) =>
    """
    Cancel a request.
    """
    match request.session
      | let s _ClientConnection tag => s.cancel(request)
    end
*/

  fun ref _get_session(url: URL, handlermaker: HandlerFactory val):
       _ClientConnection ? =>
    """
    Gets or creates an HTTP Session for the given URL.  If a new session
    is created, a new Receive Handler instance is created too.
    """
    let hs = _HostService(url.scheme, url.host, url.port.string())

    try
      // Look for an existing session
      _sessions(hs)
    else
      // or create a new session of the correct type.
      let session = match url.scheme
        | "http" =>
           _ClientConnection(_auth, hs.host, hs.service,
              None, _pipeline, handlermaker)
        | "https" =>
           _ClientConnection(_auth, hs.host, hs.service,
             _sslctx, _pipeline, handlermaker)
        else
          error
        end
      _sessions(hs) = session
      session
    end

  fun ref send_body(data: ByteSeq val, session: HTTPSession) =>
    session.write(data)

class _SessionGuard
  """
  Enforces the rule that an 'unsafe' request monopolizes the
  HTTPSession.  A 'safe' request does not modify a resource state on
  the server, and such a request has no body.
  """
  let _session: HTTPSession tag
  let _sent: List[Payload val] = List[Payload val]
  var _lastreqsafe: Bool = true
  var current: (Payload val | None) = None

  new iso create(session: HTTPSession tag) =>
    _session = session

  fun ref submit(request: Payload val) ? =>
    """
    Accept a request for transmission to the server.  This will fail if
    the request is not "safe" and the HTTPSession is busy.  Due to the
    possibly large body size, these requests can not be queued.
    """
    let thisreqsafe = request.is_safe()

    // If the channel is idle, just send this request.
    if _sent.size() == 0 then
      _lastreqsafe = thisreqsafe
      current = request
      _session(request)
      return
    end

    // Channel is not idle.  But we can send it anyway if
    // both the last request and this one are safe.
    if _lastreqsafe and thisreqsafe then
      _sent.push(request)
      _session(request)
      return
    end

    // Channel can not accept another request now.
    error

