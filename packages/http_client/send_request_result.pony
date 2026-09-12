primitive SendRequestOK
  """
  Request was serialized and sent successfully.
  """

primitive ConnectionClosed
  """
  Connection is not open, or a send failed and the connection was closed.
  """

primitive ResponsePending
  """
  A response to a previous request is still in progress.
  """

type SendRequestError is (ConnectionClosed | ResponsePending)
  """Error returned by `send_request()` when the request cannot be sent."""

type SendRequestResult is (SendRequestOK | SendRequestError)
  """Result of `send_request()`: either success or an error."""
