use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBuffer)
    ifdef not windows then
      test(_TestBroadcast)
    end

class iso _TestBuffer is UnitTest
  """
  Test adding to and reading from a Buffer.
  """
  fun name(): String => "net/Buffer"

  fun apply(h: TestHelper): TestResult ? =>
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
    h.expect_eq[U8](b.peek_u8(), 0x42)
    h.expect_eq[U16](b.peek_u16_be(1), 0xDEAD)
    h.expect_eq[U16](b.peek_u16_le(3), 0xDEAD)
    h.expect_eq[U32](b.peek_u32_be(5), 0xDEADBEEF)
    h.expect_eq[U32](b.peek_u32_le(9), 0xDEADBEEF)
    h.expect_eq[U64](b.peek_u64_be(13), 0xDEADBEEFFEEDFACE)
    h.expect_eq[U64](b.peek_u64_le(21), 0xDEADBEEFFEEDFACE)

    ifdef not ilp32 then
      h.expect_eq[U128](b.peek_u128_be(29), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
      h.expect_eq[U128](b.peek_u128_le(45), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    end

    h.expect_eq[U8](b.peek_u8(61), 'h')
    h.expect_eq[U8](b.peek_u8(62), 'i')

    // These expectations consume bytes from the head of the buffer.
    h.expect_eq[U8](b.u8(), 0x42)
    h.expect_eq[U16](b.u16_be(), 0xDEAD)
    h.expect_eq[U16](b.u16_le(), 0xDEAD)
    h.expect_eq[U32](b.u32_be(), 0xDEADBEEF)
    h.expect_eq[U32](b.u32_le(), 0xDEADBEEF)
    h.expect_eq[U64](b.u64_be(), 0xDEADBEEFFEEDFACE)
    h.expect_eq[U64](b.u64_le(), 0xDEADBEEFFEEDFACE)

    ifdef not ilp32 then
      h.expect_eq[U128](b.u128_be(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
      h.expect_eq[U128](b.u128_le(), 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    else
      b.skip(32)
    end

    h.expect_eq[String](b.line(), "hi")
    h.expect_eq[String](b.line(), "there")

    b.append(recover [as U8: 'h', 'i'] end)

    try
      b.line()
      h.assert_failed("shouldn't have a line")
    end

    b.append(recover [as U8: '!', '\n'] end)
    h.expect_eq[String](b.line(), "hi!")

    true

class _TestPing is UDPNotify
  let _h: TestHelper
  let _ip: IPAddress

  new create(h: TestHelper, ip: IPAddress) =>
    _h = h

    _ip = try
      (_, let service) = ip.name()
      let list = DNS("255.255.255.255", service)
      list(0)
    else
      _h.assert_failed("Couldn't make broadcast address")
      ip
    end

  fun ref listening(sock: UDPSocket ref) =>
    sock.set_broadcast(true)
    sock.write("ping!", _ip)

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.assert_failed("Ping: not listening")
    _h.complete(false)
    sock.dispose()

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    try
      let s = String.append(consume data)
      _h.assert_eq[String box](s, "pong!")
    end

    _h.complete(true)
    sock.dispose()

class _TestPong is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref listening(sock: UDPSocket ref) =>
    try
      let ip = sock.local_address()
      let h = _h

      if ip.ip4() then
        UDPSocket.ip4(recover _TestPing(h, ip) end)
      elseif ip.ip6() then
        UDPSocket.ip6(recover _TestPing(h, ip) end)
      else
        error
      end
    else
      _h.assert_failed("Pong: couldn't open ping")
      _h.complete(false)
      sock.dispose()
    end

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.assert_failed("Pong: not listening")
    _h.complete(false)
    sock.dispose()

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    try
      let s = String.append(consume data)
      _h.assert_eq[String box](s, "ping!")
    end

    sock.write("pong!", from)
    sock.dispose()

class iso _TestBroadcast is UnitTest
  """
  Test broadcasting with UDP.
  """
  fun name(): String => "net/Broadcast"

  fun apply(h: TestHelper): TestResult =>
    UDPSocket(recover _TestPong(h) end)
    LongTest
