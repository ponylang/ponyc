use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestReader)
    test(_TestWriter)


class iso _TestReader is UnitTest
  """
  Test adding to and reading from a Reader.
  """
  fun name(): String => "net/Reader"

  fun apply(h: TestHelper) ? =>
    let b = Reader

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


class iso _TestWriter is UnitTest
  """
  Test writing to and reading from a Writer.
  """
  fun name(): String => "net/Writer"

  fun apply(h: TestHelper) ? =>
    let b = Reader
    let wb: Writer ref = Writer

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
