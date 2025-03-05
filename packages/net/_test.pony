use "files"
use "pony_test"

primitive TimeoutValue
  fun apply(): U64 =>
    ifdef windows then
      // Windows networking is just damn slow at many things
      60_000_000_000
    else
      30_000_000_000
    end

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestTCPConnectionFailed)
    test(_TestTCPExpect)
    test(_TestTCPExpectOverBufferSize)
    test(_TestTCPMute)
    test(_TestTCPProxy)
    test(_TestTCPUnmute)
    test(_TestTCPWritev)

    // Tests below exclude windows and are listed alphabetically
    ifdef not windows then
      test(_TestTCPConnectionToClosedServerFailed)
      test(_TestTCPThrottle)
    end

    // Tests below exclude osx and are listed alphabetically
    ifdef not osx then
      test(_TestBroadcast)
    end

class \nodoc\ _TestPing is UDPNotify
  let _h: TestHelper
  let _ip: NetAddress

  new create(h: TestHelper, ip: NetAddress) =>
    _h = h

    _ip = try
      let auth = DNSAuth(h.env.root)
      (_, let service) = ip.name()?

      let list = if ip.ip4() then
        ifdef bsd then
          DNS.ip4(auth, "", service)
        else
          DNS.broadcast_ip4(auth, service)
        end
      else
        ifdef bsd then
          DNS.ip6(auth, "", service)
        else
          DNS.broadcast_ip6(auth, service)
        end
      end

      list(0)?
    else
      _h.fail("Couldn't make broadcast address")
      ip
    end

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("ping listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("ping listen")

    sock.set_broadcast(true)
    sock.write("ping!", _ip)

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    _h.complete_action("ping receive")

    let s = String .> append(consume data)
    _h.assert_eq[String box](s, "pong!")
    _h.complete(true)

class \nodoc\ _TestPong is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("pong listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("pong listen")

    sock.set_broadcast(true)
    let ip = sock.local_address()

    let h = _h
    if ip.ip4() then
      _h.dispose_when_done(
        UDPSocket.ip4(UDPAuth(h.env.root), recover _TestPing(h, ip) end))
    else
      _h.dispose_when_done(
        UDPSocket.ip6(UDPAuth(h.env.root), recover _TestPing(h, ip) end))
    end

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    _h.complete_action("pong receive")

    let s = String .> append(consume data)
    _h.assert_eq[String box](s, "ping!")
    sock.writev(
      recover val [[U8('p'); U8('o'); U8('n'); U8('g'); U8('!')]] end,
      from)

class \nodoc\ iso _TestBroadcast is UnitTest
  """
  Test broadcasting with UDP.
  """
  fun name(): String => "net/Broadcast"
  fun label(): String => "unreliable-appveyor-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("pong create")
    h.expect_action("pong listen")
    h.expect_action("ping create")
    h.expect_action("ping listen")
    h.expect_action("pong receive")
    h.expect_action("ping receive")

    h.dispose_when_done(
      UDPSocket(UDPAuth(h.env.root), recover _TestPong(h) end))

    h.long_test(TimeoutValue())

  fun ref timed_out(h: TestHelper) =>
    h.log("""
      This test may fail if you have a firewall (such as firewalld) running.
      If it does, try re-running the tests with the firewall de-activated, or
      exclude this test by passing the --exclude="net/Broadcast" option.
    """)

class \nodoc\ _TestTCP is TCPListenNotify
  """
  Run a typical TCP test consisting of a single TCPListener that accepts a
  single TCPConnection as a client, using a dynamic available listen port.
  """
  let _h: TestHelper
  var _client_conn_notify: (TCPConnectionNotify iso | None) = None
  var _server_conn_notify: (TCPConnectionNotify iso | None) = None

  new iso create(h: TestHelper) =>
    _h = h

  fun iso apply(c: TCPConnectionNotify iso, s: TCPConnectionNotify iso) =>
    _client_conn_notify = consume c
    _server_conn_notify = consume s

    let h = _h
    h.expect_action("server create")
    h.expect_action("server listen")
    h.expect_action("client create")
    h.expect_action("server accept")

    h.dispose_when_done(TCPListener(TCPListenAuth(h.env.root), consume this))
    h.complete_action("server create")

    h.long_test(TimeoutValue())

  fun ref not_listening(listen: TCPListener ref) =>
    _h.fail_action("server listen")

  fun ref listening(listen: TCPListener ref) =>
    _h.complete_action("server listen")

    try
      let notify = (_client_conn_notify = None) as TCPConnectionNotify iso^
      (let host, let port) = listen.local_address().name()?
      _h.dispose_when_done(
        TCPConnection(TCPConnectAuth(_h.env.root), consume notify, host, port))
      _h.complete_action("client create")
    else
      _h.fail_action("client create")
    end

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ ? =>
    try
      let notify = (_server_conn_notify = None) as TCPConnectionNotify iso^
      _h.complete_action("server accept")
      consume notify
    else
      _h.fail_action("server accept")
      error
    end

class \nodoc\ iso _TestTCPExpect is UnitTest
  """
  Test expecting framed data with TCP.
  """
  fun name(): String => "net/TCP.expect"
  fun label(): String => "unreliable-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("client receive")
    h.expect_action("server receive")
    h.expect_action("expect received")

    _TestTCP(h)(_TestTCPExpectNotify(h, false), _TestTCPExpectNotify(h, true))

class \nodoc\ iso _TestTCPExpectOverBufferSize is UnitTest
  """
  Test expecting framed data with TCP.
  """
  fun name(): String => "net/TCP.expectoverbuffersize"
  fun label(): String => "unreliable-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("connected")
    h.expect_action("accepted")

    _TestTCP(h)(_TestTCPExpectOverBufferSizeNotify(h),
      _TestTCPExpectOverBufferSizeNotify(h))

class \nodoc\ _TestTCPExpectNotify is TCPConnectionNotify
  let _h: TestHelper
  let _server: Bool
  var _expect: USize = 4
  var _frame: Bool = true

  new iso create(h: TestHelper, server: Bool) =>
    _server = server
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
      _send(conn, "hi there")
    else
      _h.fail("expect threw an error")
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
    else
      _h.fail("expect threw an error")
    end

  fun ref expect(conn: TCPConnection ref, qty: USize): USize =>
    _h.complete_action("expect received")
    qty

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    if _frame then
      _frame = false
      _expect = 0

      for i in data.values() do
        _expect = (_expect << 8) + i.usize()
      end
    else
      _h.assert_eq[USize](_expect, data.size())

      if _server then
        _h.complete_action("server receive")
        _h.assert_eq[String](String.from_array(data), "goodbye")
      else
        _h.complete_action("client receive")
        _h.assert_eq[String](String.from_array(data), "hi there")
        _send(conn, "goodbye")
      end

      _frame = true
      _expect = 4
    end

    try
      conn.expect(_expect)?
    else
      _h.fail("expect threw an error")
    end
    true

  fun ref _send(conn: TCPConnection ref, data: String) =>
    let len = data.size()

    var buf = recover Array[U8] end
    buf.push((len >> 24).u8())
    buf.push((len >> 16).u8())
    conn.write(consume buf)

    buf = recover Array[U8] end
    buf.push((len >> 8).u8())
    buf.push((len >> 0).u8())
    buf.append(data)
    conn.write(consume buf)

class \nodoc\ _TestTCPExpectOverBufferSizeNotify is TCPConnectionNotify
  let _h: TestHelper
  let _expect: USize = 6_000_000_000

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
      _h.fail("expect didn't error out")
    else
      _h.complete_action("accepted")
    end

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
      _h.fail("expect didn't error out")
    else
      _h.complete_action("connected")
    end

class \nodoc\ iso _TestTCPWritev is UnitTest
  """
  Test writev (and sent/sentv notification).
  """
  fun name(): String => "net/TCP.writev"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("server receive")

    _TestTCP(h)(_TestTCPWritevNotifyClient(h), _TestTCPWritevNotifyServer(h))

class \nodoc\ _TestTCPWritevNotifyClient is TCPConnectionNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter =>
    recover
      Array[ByteSeq] .> concat(data.values()) .> push(" (from client)")
    end

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.writev(recover ["hello"; ", hello"] end)

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

class \nodoc\ _TestTCPWritevNotifyServer is TCPConnectionNotify
  let _h: TestHelper
  var _buffer: String iso = recover iso String end

  new iso create(h: TestHelper) =>
    _h = h

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    _buffer.append(consume data)

    let expected = "hello, hello (from client)"

    if _buffer.size() >= expected.size() then
      let buffer: String = _buffer = recover iso String end
      _h.assert_eq[String](expected, consume buffer)
      _h.complete_action("server receive")
    end
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

class \nodoc\ iso _TestTCPMute is UnitTest
  """
  Test that the `mute` behavior stops us from reading incoming data. The
  test assumes that send/recv works correctly and that the absence of
  data received is because we muted the connection.

  Test works as follows:

  Once an incoming connection is established, we set mute on it and then
  verify that within a 2 second long test that received is not called on
  our notifier. A timeout is considering passing and received being called
  is grounds for a failure.
  """
  fun name(): String => "net/TCPMute"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("receiver accepted")
    h.expect_action("sender connected")
    h.expect_action("receiver muted")
    h.expect_action("receiver asks for data")
    h.expect_action("sender sent data")

    _TestTCP(h)(_TestTCPMuteSendNotify(h),
      _TestTCPMuteReceiveNotify(h))

  fun timed_out(h: TestHelper) =>
    h.complete(true)

class \nodoc\ _TestTCPMuteReceiveNotify is TCPConnectionNotify
  """
  Notifier to fail a test if we receive data after muting the connection.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    _h.complete_action("receiver accepted")
    conn.mute()
    _h.complete_action("receiver muted")
    conn.write("send me some data that i won't ever read")
    _h.complete_action("receiver asks for data")
    _h.dispose_when_done(conn)

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    _h.complete(false)
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("receiver connect failed")

class \nodoc\ _TestTCPMuteSendNotify is TCPConnectionNotify
  """
  Notifier that sends data back when it receives any. Used in conjunction with
  the mute receiver to verify that after muting, we don't get any data on
  to the `received` notifier on the muted connection. We only send in response
  to data from the receiver to make sure we don't end up failing due to race
  condition where the senders sends data on connect before the receiver has
  executed its mute statement.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("sender connected")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

   fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
   =>
     conn.write("it's sad that you won't ever read this")
     _h.complete_action("sender sent data")
     true

class \nodoc\ iso _TestTCPUnmute is UnitTest
  """
  Test that the `unmute` behavior will allow a connection to start reading
  incoming data again. The test assumes that `mute` works correctly and that
  after muting, `unmute` successfully reset the mute state rather than `mute`
  being broken and never actually muting the connection.

  Test works as follows:

  Once an incoming connection is established, we set mute on it, request
  that data be sent to us and then unmute the connection such that we should
  receive the return data.
  """
  fun name(): String => "net/TCPUnmute"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("receiver accepted")
    h.expect_action("sender connected")
    h.expect_action("receiver muted")
    h.expect_action("receiver asks for data")
    h.expect_action("receiver unmuted")
    h.expect_action("sender sent data")

    _TestTCP(h)(_TestTCPMuteSendNotify(h),
      _TestTCPUnmuteReceiveNotify(h))

class \nodoc\ _TestTCPUnmuteReceiveNotify is TCPConnectionNotify
  """
  Notifier to test that after muting and unmuting a connection, we get data
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    _h.complete_action("receiver accepted")
    conn.mute()
    _h.complete_action("receiver muted")
    conn.write("send me some data that i won't ever read")
    _h.complete_action("receiver asks for data")
    conn.unmute()
    _h.complete_action("receiver unmuted")

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    _h.complete(true)
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("receiver connect failed")

class \nodoc\ iso _TestTCPThrottle is UnitTest
  """
  Test that when we experience backpressure when sending that the `throttled`
  method is called on our `TCPConnectionNotify` instance.

  We do this by starting up a server connection, muting it immediately and then
  sending data to it which should trigger a throttling to happen. We don't
  start sending data til after the receiver has muted itself and sent the
  sender data. This verifies that muting has been completed before any data is
  sent as part of testing throttling.

  This test assumes that muting functionality is working correctly.
  """
  fun name(): String => "net/TCPThrottle"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("receiver accepted")
    h.expect_action("sender connected")
    h.expect_action("receiver muted")
    h.expect_action("receiver asks for data")
    h.expect_action("sender sent data")
    h.expect_action("sender throttled")

    _TestTCP(h)(_TestTCPThrottleSendNotify(h),
      _TestTCPThrottleReceiveNotify(h))

class \nodoc\ _TestTCPThrottleReceiveNotify is TCPConnectionNotify
  """
  Notifier to that mutes itself on startup. We then send data to it in order
  to trigger backpressure on the sender.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    _h.complete_action("receiver accepted")
    conn.mute()
    _h.complete_action("receiver muted")
    conn.write("send me some data that i won't ever read")
    _h.complete_action("receiver asks for data")
    _h.dispose_when_done(conn)

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("receiver connect failed")

class \nodoc\ _TestTCPThrottleSendNotify is TCPConnectionNotify
  """
  Notifier that sends data back when it receives any. Used in conjunction with
  the mute receiver to verify that after muting, we don't get any data on
  to the `received` notifier on the muted connection. We only send in response
  to data from the receiver to make sure we don't end up failing due to race
  condition where the senders sends data on connect before the receiver has
  executed its mute statement.
  """
  let _h: TestHelper
  var _throttled_yet: Bool = false

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("sender connected")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    conn.write("it's sad that you won't ever read this")
    _h.complete_action("sender sent data")
    true

  fun ref throttled(conn: TCPConnection ref) =>
    _throttled_yet = true
    _h.complete_action("sender throttled")
    _h.complete(true)

  fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq =>
    if not _throttled_yet then
      conn.write("this is more data that you won't ever read" * 10000)
    end
    data

class \nodoc\ _TestTCPProxy is UnitTest
  """
  Check that the proxy callback is called on creation of a TCPConnection.
  """
  fun name(): String => "net/TCPProxy"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("sender connected")
    h.expect_action("sender proxy request")

    _TestTCP(h)(_TestTCPProxyNotify(h),
      _TestTCPProxyNotify(h))

class \nodoc\ _TestTCPProxyNotify is TCPConnectionNotify
  let _h: TestHelper
  new iso create(h: TestHelper) =>
    _h = h

  fun ref proxy_via(host: String, service: String): (String, String) =>
    _h.complete_action("sender proxy request")
    (host, service)

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("sender connected")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

class \nodoc\ _TestTCPConnectionFailed is UnitTest
  fun name(): String => "net/TCPConnectionFailed"

  fun ref apply(h: TestHelper) =>
    h.expect_action("connection failed")

    let host = "127.0.0.1"
    let port = "7669"

    let connection = TCPConnection(
      TCPConnectAuth(h.env.root),
      object iso is TCPConnectionNotify
        let _h: TestHelper = h

        fun ref connected(conn: TCPConnection ref) =>
          _h.fail_action("connection failed")

        fun ref connect_failed(conn: TCPConnection ref) =>
          _h.complete_action("connection failed")
      end,
      host,
      port)
    h.long_test(TimeoutValue())
    h.dispose_when_done(connection)

class \nodoc\ _TestTCPConnectionToClosedServerFailed is UnitTest
  """
  Check that you can't connect to a closed listener.
  """
  fun name(): String => "net/TCPConnectionToClosedServerFailed"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connection failed")

    let listener = TCPListener(
      TCPListenAuth(h.env.root),
      object iso is TCPListenNotify
        let _h: TestHelper = h
        var host: String = "?"
        var port: String = "?"

        fun ref listening(listener: TCPListener ref) =>
          _h.complete_action("server listening")
          listener.close()

        fun ref not_listening(listener: TCPListener ref) =>
          _h.fail_action("server listening")

        fun ref closed(listener: TCPListener ref) =>
          _TCPConnectionToClosedServerFailedConnector.connect(_h, host, port)

        fun ref connected(listener: TCPListener ref)
          : TCPConnectionNotify iso^
        =>
          object iso is TCPConnectionNotify
            fun ref received(conn: TCPConnection ref, data: Array[U8] iso,
              times: USize): Bool => true
            fun ref accepted(conn: TCPConnection ref) => None
            fun ref connect_failed(conn: TCPConnection ref) => None
            fun ref closed(conn: TCPConnection ref) => None
          end
      end,
      "127.0.0.1"
    )

    h.dispose_when_done(listener)
    h.long_test(TimeoutValue())

actor \nodoc\ _TCPConnectionToClosedServerFailedConnector
  be connect(h: TestHelper, host: String, port: String) =>
    let connection = TCPConnection(
      TCPConnectAuth(h.env.root),
      object iso is TCPConnectionNotify
        let _h: TestHelper = h

        fun ref connected(conn: TCPConnection ref) =>
          _h.fail_action("client connection failed")

        fun ref connect_failed(conn: TCPConnection ref) =>
          _h.complete_action("client connection failed")
      end,
      host,
      port)
    h.dispose_when_done(connection)
