use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBuffer)
    test(_TestBroadcast)
    test(_TestTCPExpect)
    test(_TestTCPWritev)

class iso _TestBuffer is UnitTest
  """
  Test adding to and reading from a Buffer.
  """
  fun name(): String => "net/Buffer"

  fun apply(h: TestHelper) ? =>
    let b = Buffer

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

class _TestPing is UDPNotify
  let _mgr: _TestBroadcastMgr
  let _h: TestHelper
  let _ip: IPAddress

  new create(mgr: _TestBroadcastMgr, h: TestHelper, ip: IPAddress) =>
    _mgr = mgr
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

  fun ref listening(sock: UDPSocket ref) =>
    sock.set_broadcast(true)
    sock.write("ping!", _ip)

  fun ref not_listening(sock: UDPSocket ref) =>
    _mgr.fail("Ping: not listening")

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    let s = String.append(consume data)
    _h.assert_eq[String box](s, "pong!")
    _mgr.succeed()

class _TestPong is UDPNotify
  let _mgr: _TestBroadcastMgr
  let _h: TestHelper

  new create(mgr: _TestBroadcastMgr, h: TestHelper) =>
    _mgr = mgr
    _h = h

  fun ref listening(sock: UDPSocket ref) =>
    sock.set_broadcast(true)
    _mgr.pong_listening(sock.local_address())

  fun ref not_listening(sock: UDPSocket ref) =>
    _mgr.fail("Pong: not listening")

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    let s = String.append(consume data)
    _h.assert_eq[String box](s, "ping!")
    sock.writev(
      recover val [[U8('p'), U8('o'), U8('n'), U8('g'), U8('!')]] end,
      from)

actor _TestBroadcastMgr
  let _h: TestHelper
  var _pong: (UDPSocket | None) = None
  var _ping: (UDPSocket | None) = None
  var _fail: Bool = false

  new create(h: TestHelper) =>
    _h = h

    try
      _pong = UDPSocket(h.env.root as AmbientAuth,
        recover _TestPong(this, h) end)
    else
      _h.fail("could not create Pong")
      _h.complete(false)
    end

  be succeed() =>
    if not _fail then
      try (_pong as UDPSocket).dispose() end
      try (_ping as UDPSocket).dispose() end
      _h.complete(true)
    end

  be fail(msg: String) =>
    if not _fail then
      _fail = true
      try (_pong as UDPSocket).dispose() end
      try (_ping as UDPSocket).dispose() end
      _h.fail(msg)
      _h.complete(false)
    end

  be pong_listening(ip: IPAddress) =>
    if not _fail then
      let h = _h

      try
        if ip.ip4() then
          _ping = UDPSocket.ip4(h.env.root as AmbientAuth,
            recover _TestPing(this, h, ip) end)
        else
          _ping = UDPSocket.ip6(h.env.root as AmbientAuth,
            recover _TestPing(this, h, ip) end)
        end
      else
        _h.fail("could not create Ping")
        _h.complete(false)
      end
    end

class iso _TestBroadcast is UnitTest
  """
  Test broadcasting with UDP.
  """
  var _mgr: (_TestBroadcastMgr | None) = None

  fun name(): String => "net/Broadcast"

  fun ref apply(h: TestHelper) =>
    _mgr = _TestBroadcastMgr(h)
    h.long_test(2_000_000_000) // 2 second timeout

  fun timed_out(t: TestHelper) =>
    try
      (_mgr as _TestBroadcastMgr).fail("timeout")
    end

class _TestTCPExpectNotify is TCPConnectionNotify
  let _mgr: _TestMgrTCPExpect
  let _h: TestHelper
  let _server: Bool
  var _expect: USize = 4
  var _frame: Bool = true

  new iso create(mgr: _TestMgrTCPExpect, h: TestHelper, server: Bool) =>
    _server = server
    _mgr = mgr
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    conn.expect(_expect)
    _send(conn, "hi there")

  fun ref connected(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    conn.expect(_expect)

  fun ref connect_failed(conn: TCPConnection ref) =>
    _mgr.fail("couldn't connect")

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
        _h.assert_eq[String](String.from_array(data), "goodbye")
        _mgr.succeed()
      else
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

class _TestTCPExpectListen is TCPListenNotify
  let _mgr: _TestMgrTCPExpect
  let _h: TestHelper

  new iso create(mgr: _TestMgrTCPExpect, h: TestHelper) =>
    _mgr = mgr
    _h = h

  fun ref listening(listen: TCPListener ref) =>
    _mgr.listening(listen.local_address())

  fun ref not_listening(listen: TCPListener ref) =>
    _mgr.fail("not listening")

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    _TestTCPExpectNotify(_mgr, _h, true)

interface tag _TestMgr
  be succeed()
  be fail(msg: String)

actor _TestMgrNone is _TestMgr
  be succeed() => None
  be fail(msg: String) => None

actor _TestMgrTCPExpect
  let _h: TestHelper
  var _listen: (TCPListener | None) = None
  var _connect: (TCPConnection | None) = None
  var _fail: Bool = false

  new create(h: TestHelper) =>
    _h = h

    try
      _listen = TCPListener(h.env.root as AmbientAuth,
        _TestTCPExpectListen(this, h))
    else
      _h.fail("could not create TCP.expect listener")
      _h.complete(false)
    end

  be succeed() =>
    if not _fail then
      try (_listen as TCPListener).dispose() end
      try (_connect as TCPConnection).dispose() end
      _h.complete(true)
    end

  be fail(msg: String) =>
    if not _fail then
      _fail = true
      try (_listen as TCPListener).dispose() end
      try (_connect as TCPConnection).dispose() end
      _h.fail(msg)
      _h.complete(false)
    end

  be listening(ip: IPAddress) =>
    if not _fail then
      let h = _h

      try
        (let host, let service) = ip.name()
        _connect = TCPConnection.ip4(h.env.root as AmbientAuth,
          _TestTCPExpectNotify(this, h, false), host, service)
      else
        _h.fail("could not create TCP.expect connection")
        _h.complete(false)
      end
    end

class iso _TestTCPExpect is UnitTest
  """
  Test expecting framed data with TCP.
  """
  var _mgr: _TestMgr = _TestMgrNone

  fun name(): String => "net/TCP.expect"

  fun ref apply(h: TestHelper) =>
    _mgr = _TestMgrTCPExpect(h)
    h.long_test(2_000_000_000)

  fun timed_out(t: TestHelper) =>
    _mgr.fail("timeout")

actor _TestMgrTCPWritev
  let _h: TestHelper
  let _auth: TCPAuth
  let _disposables: Array[DisposableActor] = _disposables.create()
  var _disposed: Bool = false

  new create(h: TestHelper, auth: TCPAuth) =>
    _h = h
    _auth = auth
    dispose_later(TCPListener(auth, _listen_notify()))

  be dispose_later(d: DisposableActor) =>
    if _disposed then
      d.dispose()
    else
      _disposables.push(d)
    end

  be dispose() =>
    for d in _disposables.values() do d.dispose() end
    _disposed = true

  be succeed() =>
    _h.complete(true)
    dispose()

  be fail(msg: String) =>
    _h.fail(msg)
    _h.complete(false)
    dispose()

  be listening(ip: IPAddress) =>
    try
      (let host, let service) = ip.name()
      let conn = TCPConnection.ip4(_auth, _conn_notify(false), host, service)
      dispose_later(conn)
    end

  be assert_streq(expect: String, actual: String) =>
    _h.assert_eq[String](expect, actual)

  fun tag _listen_notify(): TCPListenNotify iso^ =>
    object iso is TCPListenNotify
      let _mgr: _TestMgrTCPWritev = this

      fun ref not_listening(listen: TCPListener ref) =>
        _mgr.fail("not listening")

      fun ref listening(listen: TCPListener ref) =>
        _mgr.listening(listen.local_address())

      fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
        _mgr._conn_notify(true)
    end

  fun tag _conn_notify(server: Bool): TCPConnectionNotify iso^ =>
    object iso is TCPConnectionNotify
      let _mgr: _TestMgrTCPWritev = this
      let _server: Bool = server
      var _buffer: String iso = recover iso String end

      fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq ? =>
        if not _server then
          _mgr.fail("TCPConnectionNotify.sent invoked on the client side, " +
                    "when the sentv success should have prevented it.")
        end

        let data_str = recover trn String.append(data) end
        if data_str == "ignore me" then error end

        if data_str == "replace me" then return ", hello" end

        _mgr.assert_streq("hello", consume data_str)
        data

      fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter ?=>
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
          _mgr.assert_streq(expected, consume buffer)

          if _server
          then conn.writev(recover ["hello", "ignore me", "replace me"] end)
          else _mgr.succeed()
          end
        end

      fun ref connected(conn: TCPConnection ref) =>
        if not _server then
          conn.writev(recover ["hello", ", hello"] end)
        end

      fun ref connect_failed(conn: TCPConnection ref) =>
        _mgr.fail("connect failed")
    end

class iso _TestTCPWritev is UnitTest
  """
  Test writev (and sent/sentv notification).
  """
  var _mgr: _TestMgr = _TestMgrNone

  fun name(): String => "net/TCP.writev"

  fun ref apply(h: TestHelper) ? =>
    let auth = TCPAuth(h.env.root as AmbientAuth)
    _mgr = _TestMgrTCPWritev(h, auth)
    h.long_test(2_000_000_000)

  fun timed_out(t: TestHelper) =>
    _mgr.fail("timeout")
