use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBuffer)
    test(_TestBroadcast)

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
      (_, let service) = ip.name()

      let list = if ip.ip4() then
        ifdef freebsd then
          DNS.ip4("", service)
        else
          DNS.broadcast_ip4(service)
        end
      else
        ifdef freebsd then
          DNS.ip6("", service)
        else
          DNS.broadcast_ip6(service)
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
    sock.write("pong!", from)

actor _TestBroadcastMgr
  let _h: TestHelper
  let _pong: UDPSocket
  var _ping: (UDPSocket | None) = None
  var _fail: Bool = false

  new create(h: TestHelper) =>
    _h = h
    _pong = UDPSocket(recover _TestPong(this, h) end)

  be succeed() =>
    if not _fail then
      _pong.dispose()
      try (_ping as UDPSocket).dispose() end
      _h.complete(true)
    end

  be fail(msg: String) =>
    if not _fail then
      _fail = true
      _pong.dispose()
      try (_ping as UDPSocket).dispose() end
      _h.fail(msg)
      _h.complete(false)
    end

  be pong_listening(ip: IPAddress) =>
    if not _fail then
      let h = _h

      if ip.ip4() then
        _ping = UDPSocket.ip4(recover _TestPing(this, h, ip) end)
      else
        _ping = UDPSocket.ip6(recover _TestPing(this, h, ip) end)
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

  fun timedout(t: TestHelper) =>
    try
      (_mgr as _TestBroadcastMgr).fail("timeout")
    end
