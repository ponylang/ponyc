use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestReadBuffer)
    test(_TestWriteBuffer)
    test(_TestBroadcast)
    ifdef not windows then
      test(_TestTCPExpect)
      test(_TestTCPWritev)
    end

class iso _TestReadBuffer is UnitTest
  """
  Test adding to and reading from a ReadBuffer.
  """
  fun name(): String => "net/ReadBuffer"

  fun apply(h: TestHelper) ? =>
    let b = ReadBuffer

    b.append(recover [as U8:
      0x42,
      0xDE, 0xAD,
      0xAD, 0xDE,
      0xDE, 0xAD, 0xBE, 0xEF,
      0xEF, 0xBE, 0xAD, 0xDE,
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE,
      0xCE, 0xFA, 0xED, 0xFE, 0xEF, 0xBE, 0xAD, 0xDE,
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE,
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE,
      0xCE, 0xFA, 0xED, 0xFE, 0xEF, 0xBE, 0xAD, 0xDE,
      0xCE, 0xFA, 0xED, 0xFE, 0xEF, 0xBE, 0xAD, 0xDE
      ] end)

    b.append(recover [as U8: 'h', 'i'] end)
    b.append(recover [as U8: '\n', 't', 'h', 'e'] end)
    b.append(recover [as U8: 'r', 'e', '\r', '\n'] end)

    // These expectations peek into the buffer without consuming bytes.
    h.assert_eq[U8](b.peek_u8(), 0x42)
    h.assert_eq[U16](b.peek_u16_be(1), 0xDEAD)
    h.assert_eq[U16](b.peek_u16_le(3), 0xDEAD)
    h.assert_eq[U32](b.peek_u32_be(5), 0xDEADBEEF)
    h.assert_eq[U32](b.peek_u32_le(9), 0xDEADBEEF)
    h.assert_eq[U64](b.peek_u64_be(13), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.peek_u64_le(21), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_be(29), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_le(45), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    h.assert_eq[U8](b.peek_u8(61), 'h')
    h.assert_eq[U8](b.peek_u8(62), 'i')

    // These expectations consume bytes from the head of the buffer.
    h.assert_eq[U8](b.u8(), 0x42)
    h.assert_eq[U16](b.u16_be(), 0xDEAD)
    h.assert_eq[U16](b.u16_le(), 0xDEAD)
    h.assert_eq[U32](b.u32_be(), 0xDEADBEEF)
    h.assert_eq[U32](b.u32_le(), 0xDEADBEEF)
    h.assert_eq[U64](b.u64_be(), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.u64_le(), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_be(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_le(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    h.assert_eq[String](b.line(), "hi")
    h.assert_eq[String](b.line(), "there")

    b.append(recover [as U8: 'h', 'i'] end)

    try
      b.line()
      h.fail("shouldn't have a line")
    end

    b.append(recover [as U8: '!', '\n'] end)
    h.assert_eq[String](b.line(), "hi!")

class iso _TestWriteBuffer is UnitTest
  """
  Test writing to and reading from a WriteBuffer.
  """
  fun name(): String => "net/WriteBuffer"

  fun apply(h: TestHelper) ? =>
    let b = ReadBuffer
    let wb: WriteBuffer ref = WriteBuffer

    wb.u8(0x42)
      .u16_be(0xDEAD)
      .u16_le(0xDEAD)
      .u32_be(0xDEADBEEF)
      .u32_le(0xDEADBEEF)
      .u64_be(0xDEADBEEFFEEDFACE)
      .u64_le(0xDEADBEEFFEEDFACE)
      .u128_be(0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
      .u128_le(0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    wb.write(recover [as U8: 'h', 'i'] end)
    wb.writev(recover [as Array[U8]: [as U8: '\n', 't', 'h', 'e'],
                                     [as U8: 'r', 'e', '\r', '\n']] end)

    for bs in wb.done().values() do
      try
        b.append(bs as Array[U8] val)
      end
    end

    // These expectations peek into the buffer without consuming bytes.
    h.assert_eq[U8](b.peek_u8(), 0x42)
    h.assert_eq[U16](b.peek_u16_be(1), 0xDEAD)
    h.assert_eq[U16](b.peek_u16_le(3), 0xDEAD)
    h.assert_eq[U32](b.peek_u32_be(5), 0xDEADBEEF)
    h.assert_eq[U32](b.peek_u32_le(9), 0xDEADBEEF)
    h.assert_eq[U64](b.peek_u64_be(13), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.peek_u64_le(21), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_be(29), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_le(45), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    h.assert_eq[U8](b.peek_u8(61), 'h')
    h.assert_eq[U8](b.peek_u8(62), 'i')

    // These expectations consume bytes from the head of the buffer.
    h.assert_eq[U8](b.u8(), 0x42)
    h.assert_eq[U16](b.u16_be(), 0xDEAD)
    h.assert_eq[U16](b.u16_le(), 0xDEAD)
    h.assert_eq[U32](b.u32_be(), 0xDEADBEEF)
    h.assert_eq[U32](b.u32_le(), 0xDEADBEEF)
    h.assert_eq[U64](b.u64_be(), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.u64_le(), 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_be(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_le(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    h.assert_eq[String](b.line(), "hi")
    h.assert_eq[String](b.line(), "there")

    b.append(recover [as U8: 'h', 'i'] end)

    try
      b.line()
      h.fail("shouldn't have a line")
    end

    b.append(recover [as U8: '!', '\n'] end)
    h.assert_eq[String](b.line(), "hi!")

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
      _h.dispose_when_done(TCPConnection.ip4(auth, consume notify, host, port))
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
    h.expect_action("client receive")
    h.expect_action("server receive")

    _TestTCP(h)(_TestTCPWritevNotify(h, false), _TestTCPWritevNotify(h, true))


class _TestTCPWritevNotify is TCPConnectionNotify
  let _h: TestHelper
  let _server: Bool
  var _buffer: String iso = recover iso String end

  new iso create(h: TestHelper, server: Bool) =>
    _h = h
    _server = server

  fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq ? =>
    if not _server then
      _h.fail("TCPConnectionNotify.sent invoked on the client side, " +
              "when the sentv success should have prevented it.")
    end

    let data_str = recover trn String.append(data) end
    if data_str == "ignore me" then error end

    if data_str == "replace me" then return ", hello" end

    _h.assert_eq[String]("hello", consume data_str)
    data

  fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter ? =>
    if _server then error end
    recover Array[ByteSeq].concat(data.values()).push(" (from client)") end

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _buffer.append(consume data)

    let expected =
      if _server
      then "hello, hello (from client)"
      else "hello, hello"
      end

    if _buffer.size() >= expected.size() then
      let buffer: String = _buffer = recover iso String end
      _h.assert_eq[String](expected, consume buffer)

      if _server then
        _h.complete_action("server receive")
        conn.writev(recover ["hello", "ignore me", "replace me"] end)
      else
        _h.complete_action("client receive")
      end
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect")

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.writev(recover ["hello", ", hello"] end)
