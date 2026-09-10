primitive ConnectionFailedDNS
  """
  Name resolution failed — no IP addresses could be resolved for the
  given host. No TCP connections were attempted.
  """

primitive ConnectionFailedTCP
  """
  Name resolution succeeded but all TCP connection attempts failed. At least
  one IP address was resolved, but no connection could be established.
  """

primitive ConnectionFailedSSL
  """
  The TCP connection was established but the SSL handshake failed. This
  covers both SSL session creation failures (e.g. bad `SSLContext`) and
  handshake protocol errors before `_on_connected` would have fired.
  """

primitive ConnectionFailedTimeout
  """
  The connection attempt timed out before completing. The timer covers
  TCP Happy Eyeballs and (for SSL connections) the TLS handshake. The
  timeout is configured via the `connection_timeout` parameter on the
  `client` or `ssl_client` constructor.
  """

primitive ConnectionFailedTimerError
  """
  The connection was aborted because the connect timer's ASIO event
  subscription failed. This is distinct from `ConnectionFailedTimeout` —
  the timeout didn't expire, the underlying timer could not be created
  (e.g. ENOMEM on kqueue/epoll).
  """

type ConnectionFailureReason is
  ( ConnectionFailedDNS
  | ConnectionFailedTCP
  | ConnectionFailedSSL
  | ConnectionFailedTimeout
  | ConnectionFailedTimerError )
