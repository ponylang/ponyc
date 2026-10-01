"""
Multicast receiver using the UDP multicast convenience methods.

Joins IPv4 multicast group 239.1.2.3 on the loopback interface, enables
multicast loopback, and prints every datagram received on the group.

Test with any UDP sender:
  echo hello | nc -u 239.1.2.3 5007
"""
use "net"

actor Main
  new create(env: Env) =>
    MulticastReceiver(UDPAuth(env.root), env.out)

actor MulticastReceiver is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  Joins an IPv4 multicast group and prints received datagrams.
  """
  var _udp: UDPSocket = UDPSocket.none()
  let _out: OutStream

  new create(auth: UDPAuth, out: OutStream) =>
    _out = out
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "5007", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket =>
    _udp

  fun ref _on_bound() =>
    _udp.set_multicast_interface_v4("127.0.0.1")
    _udp.join_multicast_group_v4("239.1.2.3", "127.0.0.1")
    _udp.set_multicast_loopback_v4(true)
    _out.print("Joined 239.1.2.3 on 127.0.0.1, listening on port 5007.")

  fun ref _on_bind_failure() =>
    _out.print("Couldn't bind UDP socket.")

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    _out.print("Received: " + String.from_array(consume data))
    KeepReading

  fun ref _on_closed() =>
    _out.print("Multicast receiver shut down.")
