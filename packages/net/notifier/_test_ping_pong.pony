use net = ".."
use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestNotifierPingPong is UnitTest
  fun name(): String => "net/notifier/PingPong"

  fun apply(h: TestHelper) =>
    let port = "9800"
    let pings_to_send: I32 = 100

    let listener =
      TCPListener(
        net.TCPListenAuth(h.env.root),
        recover _TestNPPListenNotify(port, pings_to_send, h) end,
        "localhost",
        port)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

class \nodoc\ _TestNPPClientNotify is ClientTCPConnectionNotify
  var _pings_to_send: I32
  let _h: TestHelper

  new create(pings_to_send: I32, h: TestHelper) =>
    _pings_to_send = pings_to_send
    _h = h

  fun ref on_connected(conn: ClientTCPConnection ref) =>
    match net.MakeBufferSize(4)
    | let e: net.BufferSize => conn.buffer_until(e)
    end
    if _pings_to_send > 0 then
      conn.write("Ping")
      _pings_to_send = _pings_to_send - 1
    end

  fun ref on_received(conn: ClientTCPConnection ref, data: Array[U8] iso):
    net.ReadAction
  =>
    if _pings_to_send > 0 then
      conn.write("Ping")
      _pings_to_send = _pings_to_send - 1
    elseif _pings_to_send == 0 then
      _h.complete(true)
    else
      _h.fail("Too many pongs received")
    end
    net.KeepReading

  fun ref on_connect_failed(conn: ClientTCPConnection ref,
    reason: net.ConnectionFailureReason)
  =>
    _h.fail("client connect_failed")
    _h.complete(false)

class \nodoc\ _TestNPPServerNotify is ServerTCPConnectionNotify
  var _pings_to_receive: I32
  let _h: TestHelper

  new create(pings_to_receive: I32, h: TestHelper) =>
    _pings_to_receive = pings_to_receive
    _h = h

  fun ref on_start_failure(conn: ServerTCPConnection ref,
    reason: net.StartFailureReason)
  =>
    None

  fun ref on_accepted(conn: ServerTCPConnection ref) =>
    match net.MakeBufferSize(4)
    | let e: net.BufferSize => conn.buffer_until(e)
    end

  fun ref on_received(conn: ServerTCPConnection ref, data: Array[U8] iso):
    net.ReadAction
  =>
    if _pings_to_receive > 0 then
      conn.write("Pong")
      _pings_to_receive = _pings_to_receive - 1
    elseif _pings_to_receive == 0 then
      conn.write("Pong")
    else
      _h.fail("Too many pings received")
    end
    net.KeepReading

class \nodoc\ _TestNPPListenNotify is TCPListenNotify
  let _port: String
  let _pings_to_receive: I32
  let _h: TestHelper

  new create(port: String, pings_to_receive: I32, h: TestHelper) =>
    _port = port
    _pings_to_receive = pings_to_receive
    _h = h

  fun ref on_listening(listen: TCPListener ref) =>
    let client =
      ClientTCPConnection(
        net.TCPConnectAuth(_h.env.root),
        recover _TestNPPClientNotify(_pings_to_receive, _h) end,
        "localhost",
        _port)
    _h.dispose_when_done(client)

  fun ref on_not_listening(listen: TCPListener ref) =>
    _h.fail("Unable to open listener")
    _h.complete(false)

  fun ref on_connected(listen: TCPListener ref):
    ServerTCPConnectionNotify iso^
  =>
    recover _TestNPPServerNotify(_pings_to_receive, _h) end
