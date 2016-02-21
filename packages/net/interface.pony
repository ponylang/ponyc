trait TCPClient
  fun connect(notify: TCPConnectionNotify iso,
    host: String, service: String, v: IPVersion = None): TCPConnection


trait TCPServer
  fun listen(notify: TCPListenNotify iso, host: String = "",
    service: String = "0", limit: USize = 0,
    v: IPVersion = None): TCPListener

trait UDPEndpoint
  fun udp_socket(notify: UDPNotify iso,
    host: String = "", service: String = "0",
    size: USize = 1024, v: IPVersion = None): UDPSocket


class NetworkInterface is (TCPClient & TCPServer & UDPEndpoint)
  """
  Access to TCP/IP networking.
  """
  let name: String

  new create(root: AmbientAuth, name': String = "") =>
    name = name'

  fun connect(notify: TCPConnectionNotify iso,
    host: String, service: String, v: IPVersion = None): TCPConnection =>
    match v
    | IPv4 => TCPConnection._ip4(consume notify, host, service where from=name)
    | IPv6 => TCPConnection._ip6(consume notify, host, service where from=name)
    else
      TCPConnection._create(consume notify, host, service where from=name)
    end

  fun listen(notify: TCPListenNotify iso, host: String = "",
    service: String = "0", limit: USize = 0,
    v: IPVersion = None): TCPListener =>
    match v
    | IPv4 => TCPListener._ip4(consume notify, host, service, limit)
    | IPv6 => TCPListener._ip6(consume notify, host, service, limit)
    else
      TCPListener._create(consume notify, host, service, limit)
    end

  fun udp_socket(notify: UDPNotify iso,
    host: String = "", service: String = "0",
    size: USize = 1024, v: IPVersion = None): UDPSocket =>
    match v
    | IPv4 => UDPSocket._ip4(consume notify, host, service, size)
    | IPv6 => UDPSocket._ip6(consume notify, host, service, size)
    else
      UDPSocket._create(consume notify, host, service, size)
    end
