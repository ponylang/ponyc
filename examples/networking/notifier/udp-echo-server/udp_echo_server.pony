"""
Minimal UDP echo server using the notifier API.

Shows the same echo-server behavior as the `udp-echo-server` example, but with
a `UDPSocketNotify` trait instead of implementing `UDPSocketActor` and
`UDPLifecycleEventReceiver` yourself. There are no user-defined actors — the
net package's concrete `UDPSocket` actor handles all I/O plumbing.

Send a datagram with any UDP tool (e.g. `echo hello | nc -u localhost 7691`)
and see it echoed back.
"""
use "net"
use notifier = "net/notifier"

actor Main
  new create(env: Env) =>
    notifier.UDPSocket(
      UDPAuth(env.root),
      recover UDPEchoNotify(env.out) end,
      "",
      "7691")

class UDPEchoNotify is notifier.UDPSocketNotify
  """
  Socket callbacks: prints status messages and echoes every received datagram
  back to its sender.
  """
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  fun ref on_bound(sock: notifier.UDPSocket ref) =>
    _out.print("UDP echo server bound.")

  fun ref on_bind_failure(sock: notifier.UDPSocket ref) =>
    _out.print("Couldn't bind UDP socket. " +
      "Perhaps try another network interface?")

  fun ref on_received(sock: notifier.UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress val)
    : ReadAction
  =>
    _out.print("Datagram received. Echoing it back.")
    match \exhaustive\ sock.send_to(consume data, from)
    | SendToOk => None
    | let e: SendToFailure =>
      _out.print("Echo send failed.")
    end
    KeepReading

  fun ref on_closed(sock: notifier.UDPSocket ref) =>
    _out.print("UDP echo server shut down.")
