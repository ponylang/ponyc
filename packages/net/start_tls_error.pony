primitive StartTLSNotConnected
  """
  The connection is not open. `start_tls()` requires an established plaintext
  connection.
  """

primitive StartTLSAlreadyTLS
  """
  The connection already has an SSL session. TLS upgrade can only be performed
  on a plaintext connection.
  """

primitive StartTLSNotReady
  """
  The connection is not in a state suitable for TLS upgrade. This can happen
  when the connection is muted, has unprocessed data in the read buffer, or
  has pending writes. Buffered read data is rejected to prevent a
  man-in-the-middle from injecting pre-TLS data that the application would
  process as post-TLS (CVE-2021-23222).
  """

primitive StartTLSSessionFailed
  """
  The SSL session could not be created from the provided `SSLContext`. The
  connection is unchanged and remains a plaintext connection.
  """

type StartTLSError is
  ( StartTLSNotConnected
  | StartTLSAlreadyTLS
  | StartTLSNotReady
  | StartTLSSessionFailed )
