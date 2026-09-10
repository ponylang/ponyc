primitive StartFailedSSL
  """
  The SSL handshake failed before the server connection could start. This
  covers both SSL session creation failures (e.g. bad `SSLContext`) and
  handshake protocol errors before `_on_started` would have fired.
  """

type StartFailureReason is StartFailedSSL
