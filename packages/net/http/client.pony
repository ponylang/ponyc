use "collections"
use "net"
use "net/ssl"
use "net/tcp"

actor Client
  """
  Manages a collection of client connections.
  """
  let _auth: TCPConnectionAuth
  let _sslctx: SSLContext
  let _pipeline: Bool
  let _clients: Map[_HostService, _ClientConnection] = _clients.create()

  new create(auth: TCPConnectionAuth, sslctx: (SSLContext | None) = None,
    pipeline: Bool = true)
  =>
    """
    Create a client for the given host and service.
    """
    _auth = auth

    _sslctx = try
      sslctx as SSLContext
    else
      recover SSLContext.set_client_verify(false) end
    end

    _pipeline = pipeline

  be apply(request: Payload val) =>
    """
    Schedule a request.
    """
    try
      let client = _get_client(request.url)
      client(request)
    end

  be cancel(request: Payload val) =>
    """
    Cancel a request.
    """
    try
      let client = _get_client(request.url)
      client.cancel(request)
    end

  fun ref _get_client(url: URL): _ClientConnection ? =>
    """
    Gets or creates a client for the given URL.
    """
    let hs = _HostService(url.scheme, url.host, url.port.string())

    try
      _clients(hs)
    else
      let client = match url.scheme
      | "http" =>
        _ClientConnection(_auth, hs.host, hs.service, None, _pipeline)
      | "https" =>
        _ClientConnection(_auth, hs.host, hs.service, _sslctx, _pipeline)
      else
        error
      end

      _clients(hs) = client
      client
    end
