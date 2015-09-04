use "collections"
use "net"
use "net/ssl"

interface TCPConnector
  """
  Capability to create a TCPConnection
  """
  fun apply(notify: TCPConnectionNotify iso,
    host: String, service: String): TCPConnection tag

class RootTCPConnector is TCPConnector
  """
  Capability to create a TCPConnection, given a Root.
  """
  let _root: Root

  new create(root: Root) =>
    _root = root

  fun apply(notify: TCPConnectionNotify iso,
    host: String, service: String): TCPConnection tag =>
    TCPConnection(_root, consume notify, host, service)


actor Client
  """
  Manages a collection of client connections.
  """
  let _sslctx: SSLContext
  let _pipeline: Bool
  let _clients: Map[_HostService, _ClientConnection] = _clients.create()
  let _mktcp: TCPConnector val

  new create(mktcp: TCPConnector val,
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
    _mktcp = mktcp

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
      | "http" => _ClientConnection(_mktcp, hs.host, hs.service, None, _pipeline)
      | "https" => _ClientConnection(_mktcp, hs.host, hs.service, _sslctx, _pipeline)
      else
        error
      end

      _clients(hs) = client
      client
    end
