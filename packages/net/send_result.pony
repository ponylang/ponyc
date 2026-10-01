primitive UDPSendOk
  """
  The datagram was handed to the OS.
  """

primitive UDPSendWouldBlock
  """
  The OS send buffer was full (`EWOULDBLOCK`/`EAGAIN`/`ENOBUFS`). The
  datagram was not sent. The caller decides whether to retry, queue, or drop.
  """

primitive UDPSendError
  """
  An unrecoverable `send` error. The datagram was not sent. The socket stays
  open.
  """

primitive UDPSendNotOpen
  """
  The socket is not connected or is already closed.
  """

type UDPSendFailure is (UDPSendWouldBlock | UDPSendError | UDPSendNotOpen)
  """
  Any `send` outcome that is not success.
  """

type UDPSendResult is (UDPSendOk | UDPSendFailure)
  """
  The outcome of a `send` call on a connected UDP socket.
  """
