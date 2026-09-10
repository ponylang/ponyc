primitive SSLAccepted
  """
  The session accepted the data. Nothing else to report.
  """

primitive SSLAuthFail
  """
  The session rejected its peer's certificate.
  """

primitive SSLReady
  """
  The handshake is complete. Application data can be sent and received.
  """

primitive SSLClosed
  """
  The peer sent `close_notify`.
  """

primitive SSLError
  """
  The session failed with a protocol error or an I/O error.
  """

primitive InvalidOperation
  """
  The operation has no meaning in the session's current state.
  """

type SSLReceiveResult is
  (SSLAccepted | SSLReady | SSLAuthFail | SSLError | InvalidOperation)
  """
  What happened when encrypted data was fed into the session via `receive`.
  """

type SSLReadResult is
  (Array[U8] iso^ | None | SSLClosed | SSLError | InvalidOperation)
  """
  What `read` produced: decrypted application data, nothing yet, a clean
  close from the peer, or an error.
  """
