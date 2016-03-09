use "net"

primitive DiscardLog
  """
  Doesn't log anything.
  """
  fun val apply(ip: String, request: Payload val, response: Payload val) =>
    None

  fun val client_ip(addr: IPAddress): String ? =>
    error
