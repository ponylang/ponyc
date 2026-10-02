## Add connected UDP mode

`ConnectedUDPSocket` binds a UDP socket to a local address and connects it to a single peer. The kernel filters incoming datagrams by source address and `send` goes to the connected peer without specifying a destination on every call.

```pony
actor MyClient
  is (ConnectedUDPSocketActor & ConnectedUDPLifecycleEventReceiver)
  var _udp: ConnectedUDPSocket = ConnectedUDPSocket.none()

  new create(auth: UDPAuth) =>
    _udp = ConnectedUDPSocket(
      auth, "localhost", "0", "192.168.1.10", "5000", this, this)

  fun ref _socket(): ConnectedUDPSocket => _udp

  fun ref _on_connected() =>
    _udp.send("hello")

  fun ref _on_bind_failure() => None
  fun ref _on_connect_failure() => None
```

`send` returns a `UDPSendResult` (`UDPSendOk` or `UDPSendFailure`) so the caller knows immediately whether the datagram was handed to the OS.

The architecture mirrors `UDPSocket`: a `ConnectedUDPSocket` class holds the state, `ConnectedUDPSocketActor` provides event plumbing, and `ConnectedUDPLifecycleEventReceiver` delivers callbacks. A notifier-style wrapper is also available at `notifier.ConnectedUDPSocket` for code that prefers the callback-style API.

Initialization has three outcomes: bind failure (`_on_bind_failure`), connect failure (`_on_connect_failure`), or success (`_on_connected`). The peer is fixed at creation — there is no disconnect or reconnect.
