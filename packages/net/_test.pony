use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBroadcast)
    test(_TestTCPWritev)
    test(_TestTCPExpect)

class _TestPing is UDPNotify
  let _h: TestHelper
  let _ip: IPAddress

  new create(h: TestHelper, ip: IPAddress) =>
    _h = h

    _ip = try
      let auth = h.env.root as AmbientAuth
      (_, let service) = ip.name()

      let list = if ip.ip4() then
        ifdef freebsd then
          DNS.ip4(auth, "", service)
        else
          DNS.broadcast_ip4(auth, service)
        end
      else
        ifdef freebsd then
          DNS.ip6(auth, "", service)
        else
          DNS.broadcast_ip6(auth, service)
        end
      end

      list(0)
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

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress) =>
    _h.complete_action("ping receive")

    let s = String.append(consume data)
    _h.assert_eq[String box](s, "pong!")
    _h.complete(true)

class _TestPong is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("pong listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("pong listen")

    sock.set_broadcast(true)
    let ip = sock.local_address()

    try
      let auth = _h.env.root as AmbientAuth
      let h = _h
      if ip.ip4() then
        _h.dispose_when_done(
          UDPSocket.ip4(auth, recover _TestPing(h, ip) end))
      else
        _h.dispose_when_done(
          UDPSocket.ip6(auth, recover _TestPing(h, ip) end))
      end
    else
      _h.fail_action("ping create")
    end

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    _h.complete_action("pong receive")

    let s = String.append(consume data)
    _h.assert_eq[String box](s, "ping!")
    sock.writev(
      recover val [[U8('p'), U8('o'), U8('n'), U8('g'), U8('!')]] end,
      from)

class iso _TestBroadcast is UnitTest
  """
  Test broadcasting with UDP.
  """
  fun name(): String => "net/Broadcast"
  fun label(): String => "unreliable-osx"

  fun ref apply(h: TestHelper) =>
    h.expect_action("pong create")
    h.expect_action("pong listen")
    h.expect_action("ping create")
    h.expect_action("ping listen")
    h.expect_action("pong receive")
    h.expect_action("ping receive")

    try
      let auth = h.env.root as AmbientAuth
      h.dispose_when_done(UDPSocket(auth, recover _TestPong(h) end))
    else
      h.fail_action("pong create")
    end

    h.long_test(2_000_000_000) // 2 second timeout

class _TestTCP is TCPListenNotify
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

    try
      let auth = h.env.root as AmbientAuth
      h.dispose_when_done(TCPListener(auth, consume this))
      h.complete_action("server create")
    else
      h.fail_action("server create")
    end

    h.long_test(2_000_000_000)

  fun ref not_listening(listen: TCPListener ref) =>
    _h.fail_action("server listen")

  fun ref listening(listen: TCPListener ref) =>
    _h.complete_action("server listen")

    try
      let auth = _h.env.root as AmbientAuth
      let notify = (_client_conn_notify = None) as TCPConnectionNotify iso^
      (let host, let port) = listen.local_address().name()
      _h.dispose_when_done(TCPConnection(auth, consume notify, host, port))
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

class iso _TestTCPExpect is UnitTest
  """
  Test expecting framed data with TCP.
  """
  fun name(): String => "net/TCP.expect"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client receive")
    h.expect_action("server receive")
    h.expect_action("expect received")

    _TestTCP(h)(_TestTCPExpectNotify(h, false), _TestTCPExpectNotify(h, true))

class _TestTCPExpectNotify is TCPConnectionNotify
  let _h: TestHelper
  let _server: Bool
  var _expect: USize = 4
  var _frame: Bool = true

  new iso create(h: TestHelper, server: Bool) =>
    _server = server
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    conn.expect(_expect)
    _send(conn, "hi there")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect")

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.set_nodelay(true)
    conn.expect(_expect)

  fun ref expect(conn: TCPConnection ref, qty: USize): USize =>
    _h.complete_action("expect received")
    qty

  fun ref received(conn: TCPConnection ref, data: Array[U8] val) =>
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

    conn.expect(_expect)

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

class iso _TestTCPWritev is UnitTest
  """
  Test writev (and sent/sentv notification).
  """
  fun name(): String => "net/TCP.writev"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("server receive")

    _TestTCP(h)(_TestTCPWritevNotifyClient(h), _TestTCPWritevNotifyServer(h))

class _TestTCPWritevNotifyClient is TCPConnectionNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter =>
    recover Array[ByteSeq].concat(data.values()).push(" (from client)") end

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.writev(recover ["hello", ", hello"] end)

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect")

class _TestTCPWritevNotifyServer is TCPConnectionNotify
  let _h: TestHelper
  var _buffer: String iso = recover iso String end

  new iso create(h: TestHelper) =>
    _h = h

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _buffer.append(consume data)

    let expected = "hello, hello (from client)"

    if _buffer.size() >= expected.size() then
      let buffer: String = _buffer = recover iso String end
      _h.assert_eq[String](expected, consume buffer)
      _h.complete_action("server receive")
    end
