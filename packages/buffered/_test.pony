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
  fun name(): String => "buffered/Reader"

  fun apply(h: TestHelper) ? =>
    let b = Reader

    // The initial bytes are all spread across multiple arrays to
    // test `else` condition when all data isn't in a single array
    // when numeric types are being read
    b.append(recover [as U8: 0x42] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xFE] end)
    b.append(recover [as U8: 0xED] end)
    b.append(recover [as U8: 0xFA] end)
    b.append(recover [as U8: 0xCE] end)
    b.append(recover [as U8: 0xCE] end)
    b.append(recover [as U8: 0xFA] end)
    b.append(recover [as U8: 0xED] end)
    b.append(recover [as U8: 0xFE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xFE] end)
    b.append(recover [as U8: 0xED] end)
    b.append(recover [as U8: 0xFA] end)
    b.append(recover [as U8: 0xCE] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xFE] end)
    b.append(recover [as U8: 0xED] end)
    b.append(recover [as U8: 0xFA] end)
    b.append(recover [as U8: 0xCE] end)
    b.append(recover [as U8: 0xCE] end)
    b.append(recover [as U8: 0xFA] end)
    b.append(recover [as U8: 0xED] end)
    b.append(recover [as U8: 0xFE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xDE] end)
    b.append(recover [as U8: 0xCE] end)
    b.append(recover [as U8: 0xFA] end)
    b.append(recover [as U8: 0xED] end)
    b.append(recover [as U8: 0xFE] end)
    b.append(recover [as U8: 0xEF] end)
    b.append(recover [as U8: 0xBE] end)
    b.append(recover [as U8: 0xAD] end)
    b.append(recover [as U8: 0xDE] end)

    // normal/contiguous data
    b.append([
      0x42
      0xDE; 0xAD
      0xAD; 0xDE
      0xDE; 0xAD; 0xBE; 0xEF
      0xEF; 0xBE; 0xAD; 0xDE
      0xDE; 0xAD; 0xBE; 0xEF; 0xFE; 0xED; 0xFA; 0xCE
      0xCE; 0xFA; 0xED; 0xFE; 0xEF; 0xBE; 0xAD; 0xDE
      0xDE; 0xAD; 0xBE; 0xEF; 0xFE; 0xED; 0xFA; 0xCE
      0xDE; 0xAD; 0xBE; 0xEF; 0xFE; 0xED; 0xFA; 0xCE
      0xCE; 0xFA; 0xED; 0xFE; 0xEF; 0xBE; 0xAD; 0xDE
      0xCE; 0xFA; 0xED; 0xFE; 0xEF; 0xBE; 0xAD; 0xDE ])

    b.append(['h'; 'i'])
    b.append(['\n'; 't'; 'h'; 'e'])
    b.append(['r'; 'e'; '\r'; '\n'])

    // These expectations peek into the buffer without consuming bytes.
    // The initial bytes are all spread across multiple arrays to
    // test `else` condition when all data isn't in a single array
    // when numeric types are being read
    h.assert_eq[U8](b.peek_u8()?, 0x42)
    h.assert_eq[U16](b.peek_u16_be(1)?, 0xDEAD)
    h.assert_eq[U16](b.peek_u16_le(3)?, 0xDEAD)
    h.assert_eq[U32](b.peek_u32_be(5)?, 0xDEADBEEF)
    h.assert_eq[U32](b.peek_u32_le(9)?, 0xDEADBEEF)
    h.assert_eq[U64](b.peek_u64_be(13)?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.peek_u64_le(21)?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_be(29)?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_le(45)?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    // These expectations peek into the buffer without consuming bytes.
    h.assert_eq[U8](b.peek_u8(61)?, 0x42)
    h.assert_eq[U16](b.peek_u16_be(62)?, 0xDEAD)
    h.assert_eq[U16](b.peek_u16_le(64)?, 0xDEAD)
    h.assert_eq[U32](b.peek_u32_be(66)?, 0xDEADBEEF)
    h.assert_eq[U32](b.peek_u32_le(70)?, 0xDEADBEEF)
    h.assert_eq[U64](b.peek_u64_be(74)?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.peek_u64_le(82)?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_be(90)?,
      0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_le(106)?,
      0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)


    h.assert_eq[U8](b.peek_u8(122)?, 'h')
    h.assert_eq[U8](b.peek_u8(123)?, 'i')


    // These expectations consume bytes from the head of the buffer.
    // The initial bytes are all spread across multiple arrays to
    // test `else` condition when all data isn't in a single array
    // when numeric types are being read
    h.assert_eq[U8](b.u8()?, 0x42)
    h.assert_eq[U16](b.u16_be()?, 0xDEAD)
    h.assert_eq[U16](b.u16_le()?, 0xDEAD)
    h.assert_eq[U32](b.u32_be()?, 0xDEADBEEF)
    h.assert_eq[U32](b.u32_le()?, 0xDEADBEEF)
    h.assert_eq[U64](b.u64_be()?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.u64_le()?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_be()?,
      0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_le()?,
      0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)


    // These expectations consume bytes from the head of the buffer.
    h.assert_eq[U8](b.u8()?, 0x42)
    h.assert_eq[U16](b.u16_be()?, 0xDEAD)
    h.assert_eq[U16](b.u16_le()?, 0xDEAD)
    h.assert_eq[U32](b.u32_be()?, 0xDEADBEEF)
    h.assert_eq[U32](b.u32_le()?, 0xDEADBEEF)
    h.assert_eq[U64](b.u64_be()?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.u64_le()?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_be()?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_le()?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    (var line: String val, _) = b.line()?
    h.assert_eq[String]("hi", line)
    try
      b.read_until(0)?
      h.fail("should fail reading until 0")
    end
    (line, _) = b.line()?
    h.assert_eq[String]("there", line)

    b.append(['h'; 'i'])

    try
      b.line()?
      h.fail("shouldn't have a line")
    end

    h.assert_eq[U8](b.u8()?, 'h')
    h.assert_eq[U8](b.u8()?, 'i')

    b.append(['!'; '\n'])
    (line, _) = b.line()?
    h.assert_eq[String](line, "!")

    b.append(['s'; 't'; 'r'; '1'])
    try
      b.read_until(0)?
      h.fail("should fail reading until 0")
    end
    b.append([0])
    b.append([
      'f'; 'i'; 'e'; 'l'; 'd'; '1'; ';'
      'f'; 'i'; 'e'; 'l'; 'd'; '2'; ';'; ';'])
    h.assert_eq[String](String.from_array(b.read_until(0)?), "str1")
    h.assert_eq[String](String.from_array(b.read_until(';')?), "field1")
    h.assert_eq[String](String.from_array(b.read_until(';')?), "field2")
    // read an empty field
    h.assert_eq[String](String.from_array(b.read_until(';')?), "")
    // the last byte is consumed by the reader
    h.assert_eq[USize](b.size(), 0)


class iso _TestWriter is UnitTest
  """
  Test writing to and reading from a Writer.
  """
  fun name(): String => "buffered/Writer"

  fun apply(h: TestHelper) ? =>
    let b = Reader
    let wb: Writer ref = Writer

    wb
      .> u8(0x42)
      .> u16_be(0xDEAD)
      .> u16_le(0xDEAD)
      .> u32_be(0xDEADBEEF)
      .> u32_le(0xDEADBEEF)
      .> u64_be(0xDEADBEEFFEEDFACE)
      .> u64_le(0xDEADBEEFFEEDFACE)
      .> u128_be(0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
      .> u128_le(0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    wb.write(['h'; 'i'])
    let chars: Array[ByteSeq] val = [
      ['\n'; 't'; 'h'; 'e']
      ['r'; 'e'; '\r'; '\n']]
    wb.writev(chars)

    for bs in wb.done().values() do
      b.append(bs)
    end

    // These expectations peek into the buffer without consuming bytes.
    h.assert_eq[U8](b.peek_u8()?, 0x42)
    h.assert_eq[U16](b.peek_u16_be(1)?, 0xDEAD)
    h.assert_eq[U16](b.peek_u16_le(3)?, 0xDEAD)
    h.assert_eq[U32](b.peek_u32_be(5)?, 0xDEADBEEF)
    h.assert_eq[U32](b.peek_u32_le(9)?, 0xDEADBEEF)
    h.assert_eq[U64](b.peek_u64_be(13)?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.peek_u64_le(21)?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_be(29)?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.peek_u128_le(45)?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    h.assert_eq[U8](b.peek_u8(61)?, 'h')
    h.assert_eq[U8](b.peek_u8(62)?, 'i')

    // These expectations consume bytes from the head of the buffer.
    h.assert_eq[U8](b.u8()?, 0x42)
    h.assert_eq[U16](b.u16_be()?, 0xDEAD)
    h.assert_eq[U16](b.u16_le()?, 0xDEAD)
    h.assert_eq[U32](b.u32_be()?, 0xDEADBEEF)
    h.assert_eq[U32](b.u32_le()?, 0xDEADBEEF)
    h.assert_eq[U64](b.u64_be()?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U64](b.u64_le()?, 0xDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_be()?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](b.u128_le()?, 0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)

    (var line: String val, _) = b.line()?
    h.assert_eq[String](line, "hi")
    (line, _) = b.line()?
    h.assert_eq[String](line, "there")

    b.append(['h'; 'i'])

    try
      b.line()?
      h.fail("shouldn't have a line")
    end

    b.append(['!'; '\n'])

    (line, _) = b.line()?
    h.assert_eq[String](line, "hi!")
