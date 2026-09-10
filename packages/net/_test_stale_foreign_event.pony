use "pony_test"

use @socket[I32](domain: I32, sock_type: I32, protocol: I32)

class \nodoc\ iso _TestStaleForeignEventDropped is UnitTest
  """
  A readiness message can arrive for a connection attempt's event after that
  event has been unsubscribed. Acting on it decrements the inflight count for
  an attempt already counted, and once that count has gone below zero a
  graceful close never sends its FIN. Deliver one and check the close still
  completes.
  """
  fun name(): String => "net/StaleForeignEventDropped"

  fun apply(h: TestHelper) =>
    h.expect_action("closed")

    let listener = _TestStaleForeignEventListener(h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStaleForeignEventClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(_h.env.root),
        "127.0.0.1",
        "7931",
        "",
        this,
        this where ip_version = IP4)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _deliver_stale_event()

  be _deliver_stale_event() =>
    // One address, so the attempt that connected took the inflight count to
    // zero on its way in. A second message for it arrives here.
    //
    // The socket is one this test owns: a regression cleans the event up a
    // second time, and the fd it closes should be ours rather than whatever
    // the runtime has open.
    let fd = @socket(I32(2), I32(1), I32(0))
    let event = PonyAsio.create_event(this, fd.u32())
    PonyAsio.unsubscribe(event)

    _tcp_connection._event_notify(event, AsioEvent.read_write())
    _tcp_connection.close()

  fun ref _on_closed() =>
    _h.complete_action("closed")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _h.fail("client could not connect")
    _h.complete(false)

actor \nodoc\ _TestStaleForeignEventListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStaleForeignEventClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "127.0.0.1",
        "7931",
        this where ip_version = IP4)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestStaleForeignEventClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client = _TestStaleForeignEventClient(_h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStaleForeignEventListener")
