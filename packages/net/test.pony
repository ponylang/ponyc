use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBuffer)

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
