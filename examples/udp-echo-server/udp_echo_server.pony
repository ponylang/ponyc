"""
Minimal UDP echo server demonstrating the net package's UDP socket.

A single actor binds a UDP socket and echoes every received datagram back to
its sender. Shows the two building blocks of a net UDP application:
UDPSocketActor for event plumbing and UDPLifecycleEventReceiver for application
callbacks.

Send a datagram with any UDP tool (e.g. `echo hello | nc -u localhost 7690`)
and see it echoed back.
"""
use "net"

actor Main
  new create(env: Env) =>
    UDPEchoServer(UDPAuth(env.root), "", "7690", env.out)

actor UDPEchoServer is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  Binds to the example's port and echoes every datagram back to its sender.
  """
  var _udp: UDPSocket = UDPSocket.none()
  let _out: OutStream

  new create(auth: UDPAuth, host: String, port: String, out: OutStream) =>
    _out = out
    _udp = UDPSocket(auth, host, port, this, this)

  fun ref _socket(): UDPSocket =>
    _udp

  fun ref _on_bound() =>
    _out.print("UDP echo server bound.")

  fun ref _on_bind_failure() =>
    _out.print("Couldn't bind UDP socket. " +
      "Perhaps try another network interface?")

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    _out.print("Datagram received. Echoing it back.")
    match \exhaustive\ _udp.send_to(consume data, from)
    | SendToOk => None
    | let e: SendToFailure =>
      _out.print("Echo send failed.")
    end
    KeepReading

  fun ref _on_closed() =>
    _out.print("UDP echo server shut down.")
