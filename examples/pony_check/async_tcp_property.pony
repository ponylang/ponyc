use "itertools"
use "net"
use "pony_check"
use "ponytest"

class _TCPSenderConnectionNotify is TCPConnectionNotify
  let _message: String

  new create(message: String) =>
    _message = message

  fun ref connected(conn: TCPConnection ref) =>
    conn.write(_message)

  fun ref connect_failed(conn: TCPConnection ref) =>
    None

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    conn.close()
    true

class val TCPSender
  """
  Class under test.

  Simple class that sends a string to a TCP server.
  """

  let _auth: AmbientAuth

  new val create(auth: AmbientAuth) =>
    _auth = auth

  fun send(
    host: String,
    port: String,
    message: String): TCPConnection tag^
  =>
    TCPConnection(
      _auth,
      recover _TCPSenderConnectionNotify(message) end,
      host,
      port)


// Test Cruft
class MyTCPConnectionNotify is TCPConnectionNotify
  let _ph: PropertyHelper
  let _expected: String

  new create(ph: PropertyHelper, expected: String) =>
    _ph = ph
    _expected = expected

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    _ph.log("received " + data.size().string() + " bytes", true)
    // assert we received the expected string
    _ph.assert_eq[USize](data.size(), _expected.size())
    for bytes in Iter[U8](_expected.values()).zip[U8]((consume data).values()) do
      _ph.assert_eq[U8](bytes._1, bytes._2)
    end
    // this will signal to the PonyCheck engine that this property is done
    // it will nonetheless execute until the end
    _ph.complete(true)
    conn.close()
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _ph.fail("connect failed")
    conn.close()

class MyTCPListenNotify is TCPListenNotify

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


  fun ref listening(listen: TCPListener ref) =>
    let address = listen.local_address()
    try
      (let host, let port) = address.name(where reversedns = None, servicename = false)?

      // now that we know the server's address we can actually send something
      _ph.dispose_when_done(
        _sender.send(host, port, _expected))
    else
      _ph.fail("could not determine server host and port")
    end

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    recover iso
      MyTCPConnectionNotify(_ph, _expected)
    end

  fun ref not_listening(listen: TCPListener ref) =>
    _ph.fail("not listening")

class _AsyncTCPSenderProperty is Property1[String]
  fun name(): String => "async/tcp_sender"

  fun params(): PropertyParams =>
    PropertyParams(where async' = true, timeout' = 5_000_000_000)

  fun gen(): Generator[String] =>
    Generators.unicode()

  fun ref property(sample: String, ph: PropertyHelper) =>
    let sender = TCPSender(ph.env.root)
    ph.dispose_when_done(
      TCPListener(
        ph.env.root,
        recover MyTCPListenNotify(sender, ph, "PONYCHECK") end,
        "127.0.0.1",
        "0"))

