interface val Logger
  """
  Handles logging request/response pairs.
  """
  fun val apply(ip: String, transfer: USize,
    request: Payload val, response: Payload val): Any
    =>
    """
    Called after the server has handled a request.
    """

interface ServerNotify
  """
  Notifications about the creation and closing of `TCPConnection`s
  within HTTP servers.
  """
  fun ref listening(server: HTTPServer ref) =>
    """
    Called when we are listening.
    """
    None

  fun ref not_listening(server: HTTPServer ref) =>
    """
    Called when we fail to listen.
    """
    None

  fun ref closed(server: HTTPServer ref) =>
    """
    Called when we stop listening.
    """
    None
