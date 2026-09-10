use "constrained_types"
use "itertools"
use "net"
use notifier = "net/notifier"
use "pony_check"
use "pony_test"

class _SenderNotify is notifier.ClientTCPConnectionNotify
  let _message: String

  new create(message: String) =>
    _message = message

  fun ref on_connected(conn: notifier.ClientTCPConnection ref) =>
    conn.write(_message)

  fun ref on_received(conn: notifier.ClientTCPConnection ref,
    data: Array[U8] iso): ReadAction
  =>
    conn.close()
    KeepReading

  fun ref on_connect_failed(conn: notifier.ClientTCPConnection ref,
    reason: ConnectionFailureReason)
  =>
    None

class val TCPSender
  """
  Class under test.

  Simple class that sends a string to a TCP server.
  """

  let _auth: TCPConnectAuth

  new val create(auth: TCPConnectAuth) =>
    _auth = auth

  fun send(
    host: String,
    port: String,
    message: String): notifier.ClientTCPConnection tag
  =>
    notifier.ClientTCPConnection(
      _auth,
      recover _SenderNotify(message) end,
      host,
      port)

class _VerifyNotify is notifier.ServerTCPConnectionNotify
  """
  Verifies that received data matches the expected string.
  """
  let _ph: PropertyHelper
  let _expected: String

  new create(ph: PropertyHelper, expected: String) =>
    _ph = ph
    _expected = expected

  fun ref on_start_failure(conn: notifier.ServerTCPConnection ref,
    reason: StartFailureReason)
  =>
    _ph.fail("server start failure")

  fun ref on_received(conn: notifier.ServerTCPConnection ref,
    data: Array[U8] iso): ReadAction
  =>
    _ph.log(
      "received " + data.size().string() + " bytes",
      true)
    _ph.assert_eq[USize](data.size(), _expected.size())
    for bytes in
      Iter[U8](_expected.values())
        .zip[U8]((consume data).values())
    do
      _ph.assert_eq[U8](bytes._1, bytes._2)
    end
    _ph.complete(true)
    conn.close()
    KeepReading

class _SenderListenNotify is notifier.TCPListenNotify
  let _sender: TCPSender
  let _ph: PropertyHelper
  let _expected: String

  new create(
    sender: TCPSender,
    ph: PropertyHelper,
    expected: String) =>
    _sender = sender
    _ph = ph
    _expected = expected

  fun ref on_listening(listen: notifier.TCPListener ref) =>
    try
      (let host, let port) =
        listen.local_address().name(
          where reversedns = None,
            servicename = false)?

      _ph.dispose_when_done(
        _sender.send(host, port, _expected))
    else
      _ph.fail(
        "could not determine server host and port")
    end

  fun ref on_connected(listen: notifier.TCPListener ref):
    notifier.ServerTCPConnectionNotify iso^
  =>
    recover iso
      _VerifyNotify(_ph, _expected)
    end

  fun ref on_not_listening(listen: notifier.TCPListener ref) =>
    _ph.fail("not listening")

class _AsyncTCPSenderProperty is Property1[String]
  fun name(): String => "async/tcp_sender"

  fun params(): PropertyParams =>
    PropertyParams(
      where async' = true, timeout' = 5_000_000_000)

  fun gen(): Generator[String] =>
    Generators.unicode()

  fun ref property(sample: String, ph: PropertyHelper) =>
    let sender =
      TCPSender(TCPConnectAuth(ph.env.root))
    ph.dispose_when_done(
      notifier.TCPListener(
        TCPListenAuth(ph.env.root),
        recover
          _SenderListenNotify(sender, ph, "PONYCHECK")
        end,
        "127.0.0.1",
        "0"))
