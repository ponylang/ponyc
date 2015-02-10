interface HTTPNotify val
  """
  Notifications for HTTP requests.
  """
  fun val apply(request: Request, response: Response): Any =>
    """
    Called when a response is available for a request.
    """
    None
