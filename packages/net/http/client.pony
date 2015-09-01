use "collections"
use "net"
use "net/ssl"

actor Client
  """
  Manages a collection of client connections.
  """
  let _sslctx: SSLContext
  let _pipeline: Bool
  let _clients: Map[_HostService, _ClientConnection] = _clients.create()
  let _root: Root // TODO: reduce scope

  new create(root: Root,
    sslctx: (SSLContext | None) = None,
    pipeline: Bool = true)
  =>
    """
    Create a client for the given host and service.
    """
    _sslctx = try
      sslctx as SSLContext
    else
      recover SSLContext.set_client_verify(false) end
    end

    _pipeline = pipeline
    _root = root

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
    let port = if url.service.size() == 0 then
      match url.scheme
      | "http" => "80"
      | "https" => "443"
      else
        error
      end
    else
      url.service
    end

    let hs = _HostService(url.scheme, url.host, port)

    try
      _clients(hs)
    else
      let client = match url.scheme
      | "http" => _ClientConnection(_root, hs.host, hs.service, None, _pipeline)
      | "https" => _ClientConnection(_root, hs.host, hs.service, _sslctx, _pipeline)
      else
        error
      end

      _clients(hs) = client
      client
    end
