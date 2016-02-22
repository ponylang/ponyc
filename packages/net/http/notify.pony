use "net"

interface val ResponseHandler
  """
  Handles responses to HTTP requests.
  """
  fun val apply(request: Payload val, response: Payload val): Any =>
    """
    Called when a response is available for a request.
    """

interface val RequestHandler
  """
  Handles requests. May dispatch based on the URL or other rules.
  """
  fun val apply(request: Payload): Any
    """
    Eventually generates a response when handed a request.
    """

interface val Logger
  """
  Handles logging request/response pairs.
  """
  fun val apply(ip: String, request: Payload val, response: Payload val): Any
  =>
    """
    Called after the server has handled a request.
    """

  fun val client_ip(addr: IPAddress): String ? =>
    """
    Convert client IP address to string.
    """

interface ServerNotify
  """
  Notifications for HTTP servers.
  """
  fun ref listening(server: Server ref) =>
    """
    Called when we are listening.
    """
    None

  fun ref not_listening(server: Server ref) =>
    """
    Called when we fail to listen.
    """
    None

  fun ref closed(server: Server ref) =>
    """
    Called when we stop listening.
    """
    None
