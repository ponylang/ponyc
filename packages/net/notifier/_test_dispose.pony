use net = ".."
use "pony_test"

class \nodoc\ iso _TestNotifierDispose is UnitTest
  """
  Disposing the listener closes it cleanly and the `closed` callback fires.
  """
  fun name(): String => "net/notifier/Dispose"

  fun apply(h: TestHelper) =>
    h.expect_action("listening")
    h.expect_action("closed")

    let listener =
      TCPListener(
        net.TCPListenAuth(h.env.root),
        recover _TestNDListenNotify(h) end,
        "localhost",
        "9804")
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

class \nodoc\ _TestNDListenNotify is TCPListenNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref on_listening(listen: TCPListener ref) =>
    _h.complete_action("listening")
    listen.close()

  fun ref on_not_listening(listen: TCPListener ref) =>
    _h.fail("Unable to open listener")
    _h.complete(false)

  fun ref on_connected(listen: TCPListener ref):
    ServerTCPConnectionNotify iso^
  =>
    _h.fail("connected should not fire")
    recover _TestNDServerNotify end

  fun ref on_closed(listen: TCPListener ref) =>
    _h.complete_action("closed")

class \nodoc\ _TestNDServerNotify is ServerTCPConnectionNotify
  fun ref on_start_failure(conn: ServerTCPConnection ref,
    reason: net.StartFailureReason)
  =>
    None
