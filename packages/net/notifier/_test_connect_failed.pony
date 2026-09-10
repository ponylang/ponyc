use net = ".."
use "pony_test"

class \nodoc\ iso _TestNotifierConnectFailed is UnitTest
  fun name(): String => "net/notifier/ConnectFailed"

  fun apply(h: TestHelper) =>
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let notify =
      recover iso
        _TestNCFClientNotify(h)
      end
    let client =
      ClientTCPConnection(
        net.TCPConnectAuth(h.env.root),
        consume notify,
        host,
        "9801")
    h.dispose_when_done(client)

    h.long_test(5_000_000_000)

class \nodoc\ _TestNCFClientNotify is ClientTCPConnectionNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref on_connected(conn: ClientTCPConnection ref) =>
    _h.fail("connected should not fire")
    _h.complete(false)

  fun ref on_connect_failed(conn: ClientTCPConnection ref,
    reason: net.ConnectionFailureReason)
  =>
    _h.complete(true)
