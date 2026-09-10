use net = ".."
use "pony_test"

class \nodoc\ iso _TestNotifierUDPEcho is UnitTest
  """
  End-to-end UDP echo through the notifier layer: one socket sends to another,
  the other echoes back.
  """
  fun name(): String => "net/notifier/UDPEcho"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let server =
      UDPSocket(
        net.UDPAuth(h.env.root),
        recover _TestNUEServerNotify(host, h) end,
        host,
        "9813")
    h.dispose_when_done(server)

class \nodoc\ _TestNUEServerNotify is UDPSocketNotify
  let _host: String
  let _h: TestHelper
  var _client: (UDPSocket tag | None) = None

  new create(host: String, h: TestHelper) =>
    _host = host
    _h = h

  fun ref on_bound(sock: UDPSocket ref) =>
    let addr = sock.local_address()
    let c =
      UDPSocket(
        net.UDPAuth(_h.env.root),
        recover _TestNUEClientNotify(addr, _h) end,
        _host,
        "0")
    _client = c

  fun ref on_bind_failure(sock: UDPSocket ref) =>
    _h.fail("Server bind failed")
    _h.complete(false)

  fun ref on_received(sock: UDPSocket ref,
    data: Array[U8] iso,
    from: net.NetAddress val)
    : net.ReadAction
  =>
    sock.send_to(consume data, from)
    net.KeepReading

  fun ref on_closed(sock: UDPSocket ref) =>
    match _client
    | let c: UDPSocket tag => c.dispose()
    end

class \nodoc\ _TestNUEClientNotify is UDPSocketNotify
  let _server_addr: net.NetAddress val
  let _h: TestHelper

  new create(server_addr: net.NetAddress val, h: TestHelper) =>
    _server_addr = server_addr
    _h = h

  fun ref on_bound(sock: UDPSocket ref) =>
    sock.send_to("ping", _server_addr)

  fun ref on_bind_failure(sock: UDPSocket ref) =>
    _h.fail("Client bind failed")
    _h.complete(false)

  fun ref on_received(sock: UDPSocket ref,
    data: Array[U8] iso,
    from: net.NetAddress val)
    : net.ReadAction
  =>
    let s = String.from_array(consume data)
    _h.assert_eq[String]("ping", s)
    sock.close()
    _h.complete(true)
    net.KeepReading
