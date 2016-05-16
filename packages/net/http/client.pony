use "collections"
use "net"
use "net/ssl"

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

  fun tag get(url: URL): Agent =>
    """
    Create an Agent for a GET request.
    """
    Agent("GET", url, this)

  fun tag post(url: URL): Agent =>
    """
    Create an Agent for a POST request.
    """
    Agent("POST", url, this)

  fun tag put(url: URL): Agent =>
    """
    Create an Agent for a PUT request.
    """
    Agent("PUT", url, this)

  fun tag head(url: URL): Agent =>
    """
    Create an Agent for a HEAD request.
    """
    Agent("HEAD", url, this)

  fun tag delete(url: URL): Agent =>
    """
    Create an Agent for a DELETE request.
    """
    Agent("DELETE", url, this)

  fun tag patch(url: URL): Agent =>
    """
    Create an Agent for a PATCH request.
    """
    Agent("PATCH", url, this)

  fun tag options(url: URL): Agent =>
    """
    Create an Agent for an OPTIONS request.
    """
    Agent("OPTIONS", url, this)

  fun tag custom_method(method: String, url: URL): Agent =>
    """
    Create an Agent for a request using a custom HTTP method.
    """
    Agent(method, url, this)
