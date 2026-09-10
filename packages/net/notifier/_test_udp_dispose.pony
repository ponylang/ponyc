use net = ".."
use "pony_test"

class \nodoc\ iso _TestNotifierUDPDispose is UnitTest
  """
  Disposing the UDP socket closes it cleanly and the `on_closed` callback
  fires.
  """
  fun name(): String => "net/notifier/UDPDispose"

  fun apply(h: TestHelper) =>
    h.expect_action("bound")
    h.expect_action("closed")

    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let sock =
      UDPSocket(
        net.UDPAuth(h.env.root),
        recover _TestNUDNotify(h) end,
        host,
        "9814")
    h.dispose_when_done(sock)

    h.long_test(5_000_000_000)

class \nodoc\ _TestNUDNotify is UDPSocketNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref on_bound(sock: UDPSocket ref) =>
    _h.complete_action("bound")
    sock.close()

  fun ref on_bind_failure(sock: UDPSocket ref) =>
    _h.fail("Unable to bind socket")
    _h.complete(false)

  fun ref on_closed(sock: UDPSocket ref) =>
    _h.complete_action("closed")
