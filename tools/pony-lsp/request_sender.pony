use "json"

trait tag ResponseNotify
  """
  Actor that gets notified when a response to a
  request has been sent.
  """

  be notify(
    method: String val,
    response: ResponseMessage val)
    """
    Handling the response to a request.
    """

trait tag RequestSender
  """
  Describes some entity that supports sending LSP
  requests.

  It needs to keep track of outgoing request ids.
  """

  be send_request(
    method: String val,
    params: (JsonObject | JsonArray | None),
    notify: (ResponseNotify | None) = None)
    """
    Send a request to the client.
    """
