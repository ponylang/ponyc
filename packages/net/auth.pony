primitive NetAuth
  """
  Authorizes access to all networking operations.
  """
  new create(from: AmbientAuth) =>
    None

primitive DNSAuth
  """
  Authorizes access to DNS resolution.
  """
  new create(from: (AmbientAuth | NetAuth)) =>
    None

primitive UDPAuth
  """
  Authorizes access to UDP socket operations.
  """
  new create(from: (AmbientAuth | NetAuth)) =>
    None

primitive TCPAuth
  """
  Authorizes access to all TCP operations.
  """
  new create(from: (AmbientAuth | NetAuth)) =>
    None

primitive TCPListenAuth
  """
  Authorizes access to TCP listen operations.
  """
  new create(from: (AmbientAuth | NetAuth | TCPAuth)) =>
    None

primitive TCPConnectAuth
  """
  Authorizes access to TCP connect operations.
  """
  new create(from: (AmbientAuth | NetAuth | TCPAuth)) =>
    None
