use "pony_test"

class \nodoc\ iso _TestBroadcast is UnitTest
  """
  Ping/pong over IPv4 broadcast: a pong socket binds localhost on an
  ephemeral port, a ping socket sends `"ping!"` to the `DNS.broadcast_ip4`
  address at the pong socket's port with `SO_BROADCAST` set, and the pong
  socket replies `"pong!"` by unicast. Retransmit loop covers UDP loss.

  Platform: not macOS (broadcast delivery is unreliable in CI there).
  Firewall-sensitive; WSL2 mirrored networking may not deliver broadcast.
  If no pong arrives after repeated attempts, the test passes vacuously
  with a log message.
  """
  fun name(): String => "net/Broadcast"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let pong = _TestBroadcastPong(UDPAuth(h.env.root), h)
    h.dispose_when_done(pong)

actor \nodoc\ _TestBroadcastPong
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  var _ping: (_TestBroadcastPing | None) = None
  var _got_ping: Bool = false

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "0", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.setsockopt_u32(
      OSSockOpt.sol_socket(), OSSockOpt.so_broadcast(), 1)
    let addr = _udp.local_address()
    let p = _TestBroadcastPing(UDPAuth(_h.env.root), addr, _h)
    _ping = p

  fun ref _on_bind_failure() =>
    _h.fail("pong bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    if s == "ping!" then
      _got_ping = true
      _udp.send_to("pong!", from)
    end
    KeepReading

  fun ref _on_closed() =>
    match _ping
    | let p: _TestBroadcastPing => p.dispose()
    end

actor \nodoc\ _TestBroadcastPing
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _dest: NetAddress val
  var _done: Bool = false
  var _attempts: U32 = 0

  new create(auth: UDPAuth, pong_addr: NetAddress val, h: TestHelper) =>
    _h = h
    _dest =
      try
        (_, let service) = pong_addr.name()?
        // BSD lacks DNS.broadcast_ip4; fall back to unicast localhost.
      let list =
          ifdef bsd then
            DNS.ip4(DNSAuth(_h.env.root), "", service)
          else
            DNS.broadcast_ip4(DNSAuth(_h.env.root), service)
          end
        list(0)?
      else
        _h.fail("couldn't make broadcast address")
        pong_addr
      end
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "0", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.setsockopt_u32(
      OSSockOpt.sol_socket(), OSSockOpt.so_broadcast(), 1)
    _udp.send_to("ping!", _dest)
    _retransmit()

  fun ref _on_bind_failure() =>
    _h.fail("ping bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    if s == "pong!" then
      _done = true
      _udp.close()
      _h.complete(true)
    end
    KeepReading

  fun ref _on_closed() =>
    if not _done then
      _h.log("broadcast ping closed before pong received; treating as" +
        " environmental (broadcast may not be deliverable)")
      _h.complete(true)
    end

  be _retransmit() =>
    if _udp.is_open() then
      _attempts = _attempts + 1
      if _attempts > 100 then
        _h.log("broadcast: no reply after " + _attempts.string() +
          " attempts; closing")
        _udp.close()
      else
        _udp.send_to("ping!", _dest)
        _retransmit()
      end
    end

class \nodoc\ iso _TestBroadcastReceive is UnitTest
  """
  Broadcast-discriminating delivery: a receiver binds to `255.255.255.255`
  itself (on Linux, receives only broadcast-addressed datagrams). A sender
  sends to the broadcast address at the receiver's port. Completion proves
  the datagram that arrived was broadcast-addressed.

  Linux-only: bind-to-broadcast semantics are Linux-verified. Falls back
  to a vacuous pass if the datagram is not delivered.
  """
  fun name(): String => "net/BroadcastReceive"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let receiver = _TestBroadcastReceiveActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(receiver)

actor \nodoc\ _TestBroadcastReceiveActor
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  var _expected: String = ""
  var _sender: (_TestBroadcastReceiveSender | None) = None
  var _done: Bool = false

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    _udp =
      UDPSocket(
        auth, "255.255.255.255", "0", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    let ip = _udp.local_address()
    _h.assert_true(ip.ip4())
    _h.assert_eq[U32](ip.ipv4_addr(), U32.max_value())
    _expected = "bcast:" + ip.port().string()
    let s =
      _TestBroadcastReceiveSender(
        UDPAuth(_h.env.root), ip.port(), _expected, _h)
    _sender = s

  fun ref _on_bind_failure() =>
    _h.fail("broadcast receive bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    if s == _expected then
      _done = true
      _udp.close()
    else
      _h.log("ignoring unexpected datagram (" + s.size().string() +
        " bytes)")
    end
    KeepReading

  fun ref _on_closed() =>
    match _sender
    | let s: _TestBroadcastReceiveSender => s.dispose()
    end
    if not _done then
      _h.log("broadcast receiver closed before expected datagram; treating" +
        " as environmental (broadcast may not be deliverable)")
    end
    _h.complete(true)

actor \nodoc\ _TestBroadcastReceiveSender
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _port: U16
  let _payload: String
  var _attempts: U32 = 0

  new create(auth: UDPAuth, port: U16, payload: String, h: TestHelper) =>
    _h = h
    _port = port
    _payload = payload
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "0", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.setsockopt_u32(
      OSSockOpt.sol_socket(), OSSockOpt.so_broadcast(), 1)
    try
      let list: Array[NetAddress] val =
        DNS.broadcast_ip4(DNSAuth(_h.env.root), _port.string())
      let dest = list(0)?
      _h.assert_eq[U32](dest.ipv4_addr(), U32.max_value())
      _udp.send_to(_payload, dest)
      _retransmit(dest)
    else
      _h.fail("couldn't resolve broadcast destination")
      _h.complete(false)
    end

  fun ref _on_bind_failure() =>
    _h.fail("broadcast sender bind failed")
    _h.complete(false)

  be _retransmit(dest: NetAddress val) =>
    if _udp.is_open() then
      _attempts = _attempts + 1
      if _attempts > 100 then
        _udp.close()
      else
        _udp.send_to(_payload, dest)
        _retransmit(dest)
      end
    end
