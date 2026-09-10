use net = ".."
use "pony_test"

class \nodoc\ iso _TestNotifierRejectConnection is UnitTest
  """
  The listener's `connected` callback raises an error, rejecting the
  connection. The listener stays open and can accept subsequent connections.
  """
  fun name(): String => "net/notifier/RejectConnection"

  fun apply(h: TestHelper) =>
    h.expect_action("listening")
    h.expect_action("second_connected")

    let listener =
      TCPListener(
        net.TCPListenAuth(h.env.root),
        recover
          _TestNRCListenNotify(
            net.TCPConnectAuth(h.env.root), h)
        end,
        "localhost",
        "9805")
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

class \nodoc\ _TestNRCListenNotify is TCPListenNotify
  let _connect_auth: net.TCPConnectAuth
  let _h: TestHelper
  var _reject_count: USize = 0

  new create(
    connect_auth: net.TCPConnectAuth,
    h: TestHelper)
  =>
    _connect_auth = connect_auth
    _h = h

  fun ref on_listening(listen: TCPListener ref) =>
    _h.complete_action("listening")
    let client =
      ClientTCPConnection(
        _connect_auth,
        recover _TestNRCClientNotify(_connect_auth, _h) end,
        "localhost",
        "9805")
    _h.dispose_when_done(client)

  fun ref on_not_listening(listen: TCPListener ref) =>
    _h.fail("Unable to open listener")
    _h.complete(false)

  fun ref on_connected(listen: TCPListener ref):
    ServerTCPConnectionNotify iso^ ?
  =>
    _reject_count = _reject_count + 1
    if _reject_count == 1 then
      error
    end
    _h.complete_action("second_connected")
    recover _TestNRCServerNotify end

class \nodoc\ _TestNRCClientNotify is ClientTCPConnectionNotify
  let _connect_auth: net.TCPConnectAuth
  let _h: TestHelper

  new create(
    connect_auth: net.TCPConnectAuth,
    h: TestHelper)
  =>
    _connect_auth = connect_auth
    _h = h

  fun ref on_connected(conn: ClientTCPConnection ref) =>
    conn.close()

  fun ref on_closed(conn: ClientTCPConnection ref) =>
    let client =
      ClientTCPConnection(
        _connect_auth,
        recover _TestNRCSecondClientNotify(_h) end,
        "localhost",
        "9805")
    _h.dispose_when_done(client)

  fun ref on_connect_failed(conn: ClientTCPConnection ref,
    reason: net.ConnectionFailureReason)
  =>
    _h.fail("client connect_failed")
    _h.complete(false)

class \nodoc\ _TestNRCSecondClientNotify is ClientTCPConnectionNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref on_connected(conn: ClientTCPConnection ref) =>
    _h.complete(true)

  fun ref on_connect_failed(conn: ClientTCPConnection ref,
    reason: net.ConnectionFailureReason)
  =>
    _h.fail("second client connect_failed")
    _h.complete(false)

class \nodoc\ _TestNRCServerNotify is ServerTCPConnectionNotify
  fun ref on_start_failure(conn: ServerTCPConnection ref,
    reason: net.StartFailureReason)
  =>
    None
