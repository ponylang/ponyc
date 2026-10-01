"""
Connected UDP socket: bind to a local port and connect to a peer.

A connected UDP socket calls POSIX `connect()` on the underlying fd, which
sets a default peer address and filters incoming datagrams by source. This
lets you use `send(data)` without specifying the peer on every call, and
the kernel drops datagrams from any other source.

This example creates an unconnected echo server and a connected client.
The client sends "ping" to the server, and the server echoes it back. The
connected client receives only datagrams from the server and nothing else.

Run it: `./connected_udp`
"""
use "net"

actor Main
  new create(env: Env) =>
    EchoServer(UDPAuth(env.root), env.out)

actor EchoServer is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  An unconnected UDP echo server that sends every datagram back to its
  sender. Starts a connected client once bound.
  """
  var _udp: UDPSocket = UDPSocket.none()
  let _auth: UDPAuth
  let _out: OutStream
  var _client: (ConnectedClient | None) = None

  new create(auth: UDPAuth, out: OutStream) =>
    _auth = auth
    _out = out
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "0", this, this)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    let addr = _udp.local_address()
    (let host, let port) =
      try addr.name()?
      else
        _out.print("Cannot resolve bound address.")
        return
      end
    _out.print("Echo server bound on " + host + ":" + port)
    _client = ConnectedClient(_auth, host, port, _out, this)

  fun ref _on_bind_failure() =>
    _out.print("Echo server bind failed.")

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let msg: String val = String.from_iso_array(consume data)
    _out.print("Server received: " + msg)
    _udp.send_to(msg, from)
    KeepReading

  fun ref _on_closed() =>
    _out.print("Echo server closed.")
    match _client
    | let c: ConnectedClient => c.dispose()
    end

actor ConnectedClient
  is (ConnectedUDPSocketActor & ConnectedUDPLifecycleEventReceiver)
  """
  A connected UDP socket that sends "ping" to the echo server and prints
  the reply.
  """
  var _udp: ConnectedUDPSocket = ConnectedUDPSocket.none()
  let _out: OutStream
  let _server: EchoServer

  new create(auth: UDPAuth,
    peer_host: String,
    peer_port: String,
    out: OutStream,
    server: EchoServer)
  =>
    _out = out
    _server = server
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp =
      ConnectedUDPSocket(auth, host, "0", peer_host, peer_port, this, this)

  fun ref _socket(): ConnectedUDPSocket => _udp

  fun ref _on_connected() =>
    _out.print("Client connected to peer.")
    match \exhaustive\ _udp.send("ping")
    | UDPSendOk =>
      _out.print("Sent: ping")
    | let _: UDPSendFailure =>
      _out.print("Send failed.")
    end

  fun ref _on_bind_failure() =>
    _out.print("Client bind failed.")
    _server.dispose()

  fun ref _on_connect_failure() =>
    _out.print("Client connect failed.")
    _server.dispose()

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Client received: " + String.from_array(consume data))
    _udp.close()
    KeepReading

  fun ref _on_closed() =>
    _out.print("Client closed.")
    _server.dispose()
