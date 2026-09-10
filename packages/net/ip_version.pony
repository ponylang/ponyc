primitive IP4
  """
  Restrict connections to IPv4 only. The listener or client will use
  `pony_os_listen_tcp4` or `pony_os_connect_tcp4`, binding or connecting
  exclusively over IPv4.
  """

primitive IP6
  """
  Restrict connections to IPv6 only. The listener or client will use
  `pony_os_listen_tcp6` or `pony_os_connect_tcp6`, binding or connecting
  exclusively over IPv6.
  """

primitive DualStack
  """
  Allow both IPv4 and IPv6 (the default). Listeners bind to both protocol
  versions. Clients use Happy Eyeballs to try both and pick the fastest.
  """

type IPVersion is (IP4 | IP6 | DualStack)
