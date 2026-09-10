use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestIP4PingPong is UnitTest
  """
  Test that IPv4-only connections work for both listener and client.
  """
  fun name(): String => "net/IP4PingPong"

  fun apply(h: TestHelper) =>
    let port = "7901"
    let pings_to_send: I32 = 100

    let listener = _TestIP4PongerListener(port, pings_to_send, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestIP4Pinger
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_send: I32
  let _h: TestHelper

  new create(port: String,
    pings_to_send: I32,
    h: TestHelper)
  =>
    _pings_to_send = pings_to_send
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "127.0.0.1",
        port,
        "",
        this,
        this where ip_version = IP4)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    if _pings_to_send > 0 then
      _tcp_connection.send("Ping")
      _pings_to_send = _pings_to_send - 1
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _pings_to_send > 0 then
      _tcp_connection.send("Ping")
      _pings_to_send = _pings_to_send - 1
    elseif _pings_to_send == 0 then
      _h.complete(true)
    else
      _h.fail("Too many pongs received")
    end
    KeepReading

actor \nodoc\ _TestIP4Ponger
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_receive: I32
  let _h: TestHelper

  new create(fd: U32,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _pings_to_receive = pings_to_receive
    _h = h

    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _pings_to_receive > 0 then
      _tcp_connection.send("Pong")
      _pings_to_receive = _pings_to_receive - 1
    elseif _pings_to_receive == 0 then
      _tcp_connection.send("Pong")
    else
      _h.fail("Too many pings received")
    end
    KeepReading

actor \nodoc\ _TestIP4PongerListener is TCPListenerActor
  let _port: String
  var _tcp_listener: TCPListener = TCPListener.none()
  var _pings_to_receive: I32
  let _h: TestHelper
  var _pinger: (_TestIP4Pinger | None) = None
  let _servers: Array[_TestIP4Ponger] = Array[_TestIP4Ponger]

  new create(port: String,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _port = port
    _pings_to_receive = pings_to_receive
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "127.0.0.1",
        _port,
        this where ip_version = IP4)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestIP4Ponger =>
    let server = _TestIP4Ponger(fd, _pings_to_receive, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_pinger as _TestIP4Pinger).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _pinger = _TestIP4Pinger(_port, _pings_to_receive, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestIP4PongerListener")

class \nodoc\ iso _TestIP6PingPong is UnitTest
  """
  Test that IPv6-only connections work for both listener and client.

  Gate: resolve ::1 with no family pinned. If no IPv6 address comes back
  on Linux, the environment has no usable IPv6 (the glibc containers CI
  uses apply AI_ADDRCONFIG) and the test passes vacuously with a log line.
  """
  fun name(): String => "net/IP6PingPong"

  fun apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)
    var has_ip6 = false
    let addrs: Array[NetAddress] val = DNS(auth, "::1", "1")
    for addr in addrs.values() do
      if addr.ip6() then has_ip6 = true end
    end
    if not has_ip6 then
      ifdef linux then
        h.log("no usable IPv6 (::1 unresolvable); skipping")
        return
      else
        h.fail("::1 did not resolve to an IPv6 address")
        return
      end
    end

    let port = "7902"
    let pings_to_send: I32 = 100

    let listener = _TestIP6PongerListener(port, pings_to_send, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestIP6Pinger
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_send: I32
  let _h: TestHelper

  new create(port: String,
    pings_to_send: I32,
    h: TestHelper)
  =>
    _pings_to_send = pings_to_send
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "::1",
        port,
        "",
        this,
        this where ip_version = IP6)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    if _pings_to_send > 0 then
      _tcp_connection.send("Ping")
      _pings_to_send = _pings_to_send - 1
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _pings_to_send > 0 then
      _tcp_connection.send("Ping")
      _pings_to_send = _pings_to_send - 1
    elseif _pings_to_send == 0 then
      _h.complete(true)
    else
      _h.fail("Too many pongs received")
    end
    KeepReading

actor \nodoc\ _TestIP6Ponger
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  var _pings_to_receive: I32
  let _h: TestHelper

  new create(fd: U32,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _pings_to_receive = pings_to_receive
    _h = h

    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _pings_to_receive > 0 then
      _tcp_connection.send("Pong")
      _pings_to_receive = _pings_to_receive - 1
    elseif _pings_to_receive == 0 then
      _tcp_connection.send("Pong")
    else
      _h.fail("Too many pings received")
    end
    KeepReading

actor \nodoc\ _TestIP6PongerListener is TCPListenerActor
  let _port: String
  var _tcp_listener: TCPListener = TCPListener.none()
  var _pings_to_receive: I32
  let _h: TestHelper
  var _pinger: (_TestIP6Pinger | None) = None
  let _servers: Array[_TestIP6Ponger] = Array[_TestIP6Ponger]

  new create(port: String,
    pings_to_receive: I32,
    h: TestHelper)
  =>
    _port = port
    _pings_to_receive = pings_to_receive
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "::1",
        _port,
        this where ip_version = IP6)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestIP6Ponger =>
    let server = _TestIP6Ponger(fd, _pings_to_receive, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_pinger as _TestIP6Pinger).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _pinger = _TestIP6Pinger(_port, _pings_to_receive, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestIP6PongerListener")
