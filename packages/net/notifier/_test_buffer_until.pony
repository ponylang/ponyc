use net = ".."
use "constrained_types"
use "pony_test"

class \nodoc\ iso _TestNotifierBufferUntil is UnitTest
  fun name(): String => "net/notifier/BufferUntil"

  fun apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connected")
    h.expect_action("expected data received")

    let listener =
      TCPListener(
        net.TCPListenAuth(h.env.root),
        recover _TestNBUListenNotify(h) end,
        "localhost",
        "9803")
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

class \nodoc\ _TestNBUClientNotify is ClientTCPConnectionNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref on_connected(conn: ClientTCPConnection ref) =>
    _h.complete_action("client connected")
    conn.write("hi there, how are you???")

  fun ref on_connect_failed(conn: ClientTCPConnection ref,
    reason: net.ConnectionFailureReason)
  =>
    _h.fail("client connect_failed")
    _h.complete(false)

class \nodoc\ _TestNBUServerNotify is ServerTCPConnectionNotify
  let _h: TestHelper
  var _received_count: U8 = 0

  new create(h: TestHelper) =>
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
    _received_count = _received_count + 1

    if _received_count == 1 then
      _h.assert_eq[String]("hi t", String.from_array(consume data))
    elseif _received_count == 2 then
      _h.assert_eq[String]("here", String.from_array(consume data))
    elseif _received_count == 3 then
      _h.assert_eq[String](", ho", String.from_array(consume data))
    elseif _received_count == 4 then
      _h.assert_eq[String]("w ar", String.from_array(consume data))
    elseif _received_count == 5 then
      _h.assert_eq[String]("e yo", String.from_array(consume data))
    elseif _received_count == 6 then
      _h.assert_eq[String]("u???", String.from_array(consume data))
      _h.complete_action("expected data received")
      conn.close()
    end
    net.KeepReading

class \nodoc\ _TestNBUListenNotify is TCPListenNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref on_listening(listen: TCPListener ref) =>
    _h.complete_action("server listening")
    let client =
      ClientTCPConnection(
        net.TCPConnectAuth(_h.env.root),
        recover _TestNBUClientNotify(_h) end,
        "localhost",
        "9803")
    _h.dispose_when_done(client)

  fun ref on_not_listening(listen: TCPListener ref) =>
    _h.fail("Unable to open listener")
    _h.complete(false)

  fun ref on_connected(listen: TCPListener ref):
    ServerTCPConnectionNotify iso^
  =>
    recover _TestNBUServerNotify(_h) end
