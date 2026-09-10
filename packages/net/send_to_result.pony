primitive SendToOk
  """
  The datagram was handed to the OS.
  """

primitive SendToWouldBlock
  """
  The OS send buffer was full (`EWOULDBLOCK`/`EAGAIN`/`ENOBUFS`). The
  datagram was not sent. The caller decides whether to retry, queue, or drop.
  """

primitive SendToError
  """
  An unrecoverable `sendto` error (e.g. `ENETUNREACH`). The datagram was not
  sent. The socket stays open.
  """

primitive SendToNotOpen
  """
  The socket is not bound or is already closed.
  """

type SendToFailure is (SendToWouldBlock | SendToError | SendToNotOpen)
  """
  Any `send_to` outcome that is not success.
  """

type SendToResult is (SendToOk | SendToFailure)
  """
  The outcome of a `send_to` call.
  """
