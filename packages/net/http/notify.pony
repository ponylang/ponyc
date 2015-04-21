interface ResponseHandler val
  """
  Handles responses to HTTP requests.
  """
  fun val apply(request: Payload, response: Payload): Any =>
    """
    Called when a response is available for a request.
    """
    None

interface RequestHandler val
  """
  Handles requests to a specific path prefix.

  TODO: inform it when it is mapped or unmapped to a path?
  """
  fun val apply(request: Payload): Any
    """
    Eventually generates a response when handed a request.
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
