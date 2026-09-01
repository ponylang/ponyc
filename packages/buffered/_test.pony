use "pony_test"
use "pony_check"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestReader)
    test(_TestWriter)
    test(Property1UnitTest[U8](_PropU8Roundtrip))
    test(Property1UnitTest[U16](_PropU16Roundtrip))
    test(Property1UnitTest[U32](_PropU32Roundtrip))
    test(Property1UnitTest[U64](_PropU64Roundtrip))
    test(Property1UnitTest[U128](_PropU128Roundtrip))
    test(Property1UnitTest[(U16, U16)](_PropU16ChunkedRoundtrip))
    test(Property1UnitTest[(U32, U16)](_PropU32ChunkedRoundtrip))
    test(Property1UnitTest[(U64, U16)](_PropU64ChunkedRoundtrip))
    test(Property1UnitTest[(U128, U16)](_PropU128ChunkedRoundtrip))
    test(Property1UnitTest[U128](_PropPeekConsistency))
    test(Property1UnitTest[(U128, U16)](_PropPeekChunked))
    test(Property1UnitTest[Array[U8]](_PropBlockRoundtrip))
    test(Property1UnitTest[(Array[U8], U16)](_PropBlockChunked))
    test(Property1UnitTest[(Array[U8], U8)](_PropSkipRead))
    test(Property1UnitTest[(String, Bool)](_PropLineRoundtrip))
    test(Property1UnitTest[(Array[U8], U8)](_PropReadUntil))
    test(Property1UnitTest[(U64, U64)](_PropWriterDoneReset))
    test(Property1UnitTest[U16](_PropEndianCrossCheckU16))
    test(Property1UnitTest[U32](_PropEndianCrossCheckU32))
    test(Property1UnitTest[U64](_PropEndianCrossCheckU64))
    test(Property1UnitTest[U128](_PropEndianCrossCheckU128))
    test(Property1UnitTest[U32](_PropEndianCrossCheckF32))
    test(Property1UnitTest[U64](_PropEndianCrossCheckF64))
    test(Property1UnitTest[((U8, U16), (U32, U64))](
      _PropMultiValueRoundtrip))
    test(Property1UnitTest[Array[U8]](_PropWriterSize))
    test(Property1UnitTest[Array[U8]](_PropEmptyAppend))

class \nodoc\ iso _TestReader is UnitTest
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
    b.append(
      [ 0x42
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
    h.assert_eq[U128](
      b.peek_u128_be(90)?,
      0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](
      b.peek_u128_le(106)?,
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
    h.assert_eq[U128](
      b.u128_be()?,
      0xDEADBEEFFEEDFACEDEADBEEFFEEDFACE)
    h.assert_eq[U128](
      b.u128_le()?,
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

    h.assert_eq[String](b.line()?, "hi")
    try
      b.read_until(0)?
      h.fail("should fail reading until 0")
    end
    h.assert_eq[String](b.line()?, "there")

    b.append(['h'; 'i'])

    try
      b.line()?
      h.fail("shouldn't have a line")
    end

    h.assert_eq[U8](b.u8()?, 'h')
    h.assert_eq[U8](b.u8()?, 'i')

    b.append(['!'; '\n'])
    h.assert_eq[String](b.line()?, "!")

    b.append(['s'; 't'; 'r'; '1'])
    try
      b.read_until(0)?
      h.fail("should fail reading until 0")
    end
    b.append([0])
    b.append(
      [ 'f'; 'i'; 'e'; 'l'; 'd'; '1'; ';'
        'f'; 'i'; 'e'; 'l'; 'd'; '2'; ';'; ';'])
    h.assert_eq[String](String.from_array(b.read_until(0)?), "str1")
    h.assert_eq[String](String.from_array(b.read_until(';')?), "field1")
    h.assert_eq[String](String.from_array(b.read_until(';')?), "field2")
    // read an empty field
    h.assert_eq[String](String.from_array(b.read_until(';')?), "")
    // the last byte is consumed by the reader
    h.assert_eq[USize](b.size(), 0)

class \nodoc\ iso _TestWriter is UnitTest
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
    wb.writev(
      [ ['\n'; 't'; 'h'; 'e']
        ['r'; 'e'; '\r'; '\n']])

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

    h.assert_eq[String](b.line()?, "hi")
    h.assert_eq[String](b.line()?, "there")

    b.append(['h'; 'i'])

    try
      b.line()?
      h.fail("shouldn't have a line")
    end

    b.append(['!'; '\n'])
    h.assert_eq[String](b.line()?, "hi!")

primitive \nodoc\ _BH
  fun writer_bytes(chunks: Array[ByteSeq] val): Array[U8] val =>
    recover val
      let a = Array[U8]
      for chunk in chunks.values() do
        match \exhaustive\ chunk
        | let s: String val => a.append(s.array())
        | let b': Array[U8] val => a.append(b')
        end
      end
      a
    end

  fun ref_to_val(data: Array[U8] ref): Array[U8] val =>
    let wb = Writer
    for b in data.values() do wb.u8(b) end
    writer_bytes(wb.done())

  fun split_chunks(flat: Array[U8] val, mask: U16)
    : Array[Array[U8] val] val
  =>
    if flat.size() == 0 then
      return recover val Array[Array[U8] val] end
    end
    recover val
      let chunks = Array[Array[U8] val]
      var start: USize = 0
      var i: USize = 0
      let num_bytes = flat.size()
      while i < (num_bytes - 1) do
        if (mask and (U16(1) << i.u16())) != 0 then
          let chunk =
            recover val
              let a = Array[U8]((i + 1) - start)
              var j = start
              while j <= i do
                try a.push(flat(j)?) else _Unreachable() end
                j = j + 1
              end
              a
            end
          chunks.push(chunk)
          start = i + 1
        end
        i = i + 1
      end
      let last =
        recover val
          let a = Array[U8](num_bytes - start)
          var j = start
          while j < num_bytes do
            try a.push(flat(j)?) else _Unreachable() end
            j = j + 1
          end
          a
        end
      chunks.push(last)
      chunks
    end

class \nodoc\ iso _PropU8Roundtrip is Property1[U8]
  fun name(): String => "buffered/PropU8Roundtrip"

  fun gen(): Generator[U8] =>
    Generators.frequency[U8](
      [ as WeightedGenerator[U8]:
        (8, Generators.u8())
        (1, Generators.one_of[U8]([as U8: 0; 1; 0x7F; 0x80; 0xFF]))
      ])

  fun ref property(v: U8, ph: PropertyHelper) ? =>
    let wb = Writer
    wb.u8(v)
    let rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U8](rb.u8()?, v)
    ph.assert_eq[USize](rb.size(), 0)

class \nodoc\ iso _PropU16Roundtrip is Property1[U16]
  fun name(): String => "buffered/PropU16Roundtrip"

  fun gen(): Generator[U16] =>
    Generators.frequency[U16](
      [ as WeightedGenerator[U16]:
        (8, Generators.u16())
        (1, Generators.one_of[U16](
          [as U16: 0; 1; 0xFF; 0x100; 0x7FFF; 0x8000; 0xFFFF]))
      ])

  fun ref property(v: U16, ph: PropertyHelper) ? =>
    // U16 BE
    var wb = Writer
    wb.u16_be(v)
    var rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U16](rb.u16_be()?, v)

    // U16 LE
    wb = Writer
    wb.u16_le(v)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U16](rb.u16_le()?, v)

    // I16 BE
    wb = Writer
    wb.i16_be(v.i16())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I16](rb.i16_be()?, v.i16())

    // I16 LE
    wb = Writer
    wb.i16_le(v.i16())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I16](rb.i16_le()?, v.i16())

class \nodoc\ iso _PropU32Roundtrip is Property1[U32]
  fun name(): String => "buffered/PropU32Roundtrip"

  fun gen(): Generator[U32] =>
    Generators.frequency[U32](
      [ as WeightedGenerator[U32]:
        (8, Generators.u32())
        (1, Generators.one_of[U32](
          [ as U32: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
            0x7FFFFFFF; 0x80000000; 0xFFFFFFFF]))
      ])

  fun ref property(v: U32, ph: PropertyHelper) ? =>
    // U32 BE
    var wb = Writer
    wb.u32_be(v)
    var rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U32](rb.u32_be()?, v)

    // U32 LE
    wb = Writer
    wb.u32_le(v)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U32](rb.u32_le()?, v)

    // I32 BE
    wb = Writer
    wb.i32_be(v.i32())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I32](rb.i32_be()?, v.i32())

    // I32 LE
    wb = Writer
    wb.i32_le(v.i32())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I32](rb.i32_le()?, v.i32())

    // F32 BE (compare bits to handle NaN)
    let f = F32.from_bits(v)
    wb = Writer
    wb.f32_be(f)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U32](rb.f32_be()?.bits(), v)

    // F32 LE
    wb = Writer
    wb.f32_le(f)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U32](rb.f32_le()?.bits(), v)

class \nodoc\ iso _PropU64Roundtrip is Property1[U64]
  fun name(): String => "buffered/PropU64Roundtrip"

  fun gen(): Generator[U64] =>
    Generators.frequency[U64](
      [ as WeightedGenerator[U64]:
        (8, Generators.u64())
        (1, Generators.one_of[U64](
          [ as U64: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
            0xFFFFFFFF; 0x100000000; U64.max_value()]))
      ])

  fun ref property(v: U64, ph: PropertyHelper) ? =>
    // U64 BE
    var wb = Writer
    wb.u64_be(v)
    var rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U64](rb.u64_be()?, v)

    // U64 LE
    wb = Writer
    wb.u64_le(v)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U64](rb.u64_le()?, v)

    // I64 BE
    wb = Writer
    wb.i64_be(v.i64())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I64](rb.i64_be()?, v.i64())

    // I64 LE
    wb = Writer
    wb.i64_le(v.i64())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I64](rb.i64_le()?, v.i64())

    // F64 BE (compare bits)
    let f = F64.from_bits(v)
    wb = Writer
    wb.f64_be(f)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U64](rb.f64_be()?.bits(), v)

    // F64 LE
    wb = Writer
    wb.f64_le(f)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U64](rb.f64_le()?.bits(), v)

class \nodoc\ iso _PropU128Roundtrip is Property1[U128]
  fun name(): String => "buffered/PropU128Roundtrip"

  fun gen(): Generator[U128] =>
    Generators.frequency[U128](
      [ as WeightedGenerator[U128]:
        (8, Generators.u128())
        (1, Generators.one_of[U128](
          [ as U128: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
            0xFFFFFFFF; 0x100000000
            0xFFFFFFFFFFFFFFFF; U128.max_value()]))
      ])

  fun ref property(v: U128, ph: PropertyHelper) ? =>
    // U128 BE
    var wb = Writer
    wb.u128_be(v)
    var rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U128](rb.u128_be()?, v)

    // U128 LE
    wb = Writer
    wb.u128_le(v)
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[U128](rb.u128_le()?, v)

    // I128 BE
    wb = Writer
    wb.i128_be(v.i128())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I128](rb.i128_be()?, v.i128())

    // I128 LE
    wb = Writer
    wb.i128_le(v.i128())
    rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[I128](rb.i128_le()?, v.i128())

class \nodoc\ iso _PropU16ChunkedRoundtrip is Property1[(U16, U16)]
  """
  Tests the slow path: bytes split across multiple chunks.
  """
  fun name(): String => "buffered/PropU16ChunkedRoundtrip"

  fun gen(): Generator[(U16, U16)] =>
    Generators.zip2[U16, U16](
      Generators.frequency[U16](
        [ as WeightedGenerator[U16]:
          (8, Generators.u16())
          (1, Generators.one_of[U16](
            [as U16: 0; 1; 0xFF; 0x100; 0x7FFF; 0x8000; 0xFFFF]))
        ]),
      Generators.u16())

  fun ref property(arg: (U16, U16), ph: PropertyHelper) ? =>
    (let v, let mask) = arg
    // BE
    var wb = Writer
    wb.u16_be(v)
    var flat = _BH.writer_bytes(wb.done())
    var rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U16](rb.u16_be()?, v)

    // LE
    wb = Writer
    wb.u16_le(v)
    flat = _BH.writer_bytes(wb.done())
    rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U16](rb.u16_le()?, v)

class \nodoc\ iso _PropU32ChunkedRoundtrip is Property1[(U32, U16)]
  fun name(): String => "buffered/PropU32ChunkedRoundtrip"

  fun gen(): Generator[(U32, U16)] =>
    Generators.zip2[U32, U16](
      Generators.frequency[U32](
        [ as WeightedGenerator[U32]:
          (8, Generators.u32())
          (1, Generators.one_of[U32](
            [ as U32: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
              0x7FFFFFFF; 0x80000000; 0xFFFFFFFF]))
        ]),
      Generators.u16())

  fun ref property(arg: (U32, U16), ph: PropertyHelper) ? =>
    (let v, let mask) = arg
    // BE
    var wb = Writer
    wb.u32_be(v)
    var flat = _BH.writer_bytes(wb.done())
    var rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U32](rb.u32_be()?, v)

    // LE
    wb = Writer
    wb.u32_le(v)
    flat = _BH.writer_bytes(wb.done())
    rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U32](rb.u32_le()?, v)

class \nodoc\ iso _PropU64ChunkedRoundtrip is Property1[(U64, U16)]
  fun name(): String => "buffered/PropU64ChunkedRoundtrip"

  fun gen(): Generator[(U64, U16)] =>
    Generators.zip2[U64, U16](
      Generators.frequency[U64](
        [ as WeightedGenerator[U64]:
          (8, Generators.u64())
          (1, Generators.one_of[U64](
            [ as U64: 0; 1; 0xFF; 0xFFFF; 0xFFFFFFFF
              0x100000000; U64.max_value()]))
        ]),
      Generators.u16())

  fun ref property(arg: (U64, U16), ph: PropertyHelper) ? =>
    (let v, let mask) = arg
    // BE
    var wb = Writer
    wb.u64_be(v)
    var flat = _BH.writer_bytes(wb.done())
    var rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U64](rb.u64_be()?, v)

    // LE
    wb = Writer
    wb.u64_le(v)
    flat = _BH.writer_bytes(wb.done())
    rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U64](rb.u64_le()?, v)

class \nodoc\ iso _PropU128ChunkedRoundtrip is Property1[(U128, U16)]
  fun name(): String => "buffered/PropU128ChunkedRoundtrip"

  fun gen(): Generator[(U128, U16)] =>
    Generators.zip2[U128, U16](
      Generators.frequency[U128](
        [ as WeightedGenerator[U128]:
          (8, Generators.u128())
          (1, Generators.one_of[U128](
            [ as U128: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
              0xFFFFFFFF; 0x100000000
              0xFFFFFFFFFFFFFFFF; U128.max_value()]))
        ]),
      Generators.u16())

  fun ref property(arg: (U128, U16), ph: PropertyHelper) ? =>
    (let v, let mask) = arg
    // BE
    var wb = Writer
    wb.u128_be(v)
    var flat = _BH.writer_bytes(wb.done())
    var rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U128](rb.u128_be()?, v)

    // LE
    wb = Writer
    wb.u128_le(v)
    flat = _BH.writer_bytes(wb.done())
    rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end
    ph.assert_eq[U128](rb.u128_le()?, v)

class \nodoc\ iso _PropPeekConsistency is Property1[U128]
  """
  Peek returns correct values without consuming data.
  """
  fun name(): String => "buffered/PropPeekConsistency"

  fun gen(): Generator[U128] =>
    Generators.frequency[U128](
      [ as WeightedGenerator[U128]:
        (8, Generators.u128())
        (1, Generators.one_of[U128]([as U128: 0; 1; U128.max_value()]))
      ])

  fun ref property(v: U128, ph: PropertyHelper) ? =>
    let wb = Writer
    wb.u128_be(v)
    let rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end

    let size_before = rb.size()

    // Peek at individual bytes (big-endian: byte 0 is most significant)
    var i: USize = 0
    while i < 16 do
      let shift = ((15 - i) * 8).u128()
      let expected = (v >> shift).u8()
      ph.assert_eq[U8](rb.peek_u8(i)?, expected)
      i = i + 1
    end

    // Peek at wider values
    ph.assert_eq[U16](rb.peek_u16_be()?, (v >> 112).u16())
    ph.assert_eq[U32](rb.peek_u32_be()?, (v >> 96).u32())
    ph.assert_eq[U64](rb.peek_u64_be()?, (v >> 64).u64())
    ph.assert_eq[U64](rb.peek_u64_be(8)?, v.u64())
    ph.assert_eq[U128](rb.peek_u128_be()?, v)

    // LE peek from LE-written data
    let wb2 = Writer
    wb2.u128_le(v)
    let rb2 = Reader
    for chunk in (wb2.done()).values() do rb2.append(chunk) end

    ph.assert_eq[U16](rb2.peek_u16_le()?, v.u16())
    ph.assert_eq[U32](rb2.peek_u32_le()?, v.u32())
    ph.assert_eq[U64](rb2.peek_u64_le()?, v.u64())
    ph.assert_eq[U64](rb2.peek_u64_le(8)?, (v >> 64).u64())
    ph.assert_eq[U128](rb2.peek_u128_le()?, v)

    // Size unchanged after all peeks
    ph.assert_eq[USize](rb.size(), size_before)
    ph.assert_eq[USize](rb2.size(), size_before)

    // Consuming read gives same value
    ph.assert_eq[U128](rb.u128_be()?, v)
    ph.assert_eq[USize](rb.size(), 0)

class \nodoc\ iso _PropPeekChunked is Property1[(U128, U16)]
  """
  Peek across chunk boundaries, through _peek_byte's chunk traversal.
  """
  fun name(): String => "buffered/PropPeekChunked"

  fun gen(): Generator[(U128, U16)] =>
    Generators.zip2[U128, U16](
      Generators.frequency[U128](
        [ as WeightedGenerator[U128]:
          (8, Generators.u128())
          (1, Generators.one_of[U128](
            [as U128: 0; 1; U128.max_value()]))
        ]),
      Generators.u16())

  fun ref property(arg: (U128, U16), ph: PropertyHelper) ? =>
    (let v, let mask) = arg
    let wb = Writer
    wb.u128_be(v)
    let flat = _BH.writer_bytes(wb.done())
    let rb = Reader
    for c in _BH.split_chunks(flat, mask).values() do rb.append(c) end

    // Peek at each byte across arbitrary chunk boundaries
    var i: USize = 0
    while i < 16 do
      let shift = ((15 - i) * 8).u128()
      let expected = (v >> shift).u8()
      ph.assert_eq[U8](rb.peek_u8(i)?, expected)
      i = i + 1
    end

    ph.assert_eq[U128](rb.peek_u128_be()?, v)
    ph.assert_eq[USize](rb.size(), 16)

class \nodoc\ iso _PropBlockRoundtrip is Property1[Array[U8]]
  fun name(): String => "buffered/PropBlockRoundtrip"

  fun gen(): Generator[Array[U8]] =>
    Generators.array_of[U8](Generators.u8() where min = 1, max = 200)

  fun ref property(data: Array[U8], ph: PropertyHelper) ? =>
    let data_val = _BH.ref_to_val(data)
    let wb = Writer
    wb.write(data_val)
    let rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    let result = rb.block(data_val.size())?
    ph.assert_eq[USize](result.size(), data_val.size())
    var i: USize = 0
    while i < data_val.size() do
      ph.assert_eq[U8](result(i)?, data_val(i)?)
      i = i + 1
    end
    ph.assert_eq[USize](rb.size(), 0)

class \nodoc\ iso _PropBlockChunked is Property1[(Array[U8], U16)]
  """
  Block read across chunk boundaries, through the block assembly loop.
  """
  fun name(): String => "buffered/PropBlockChunked"

  fun gen(): Generator[(Array[U8], U16)] =>
    Generators.zip2[Array[U8], U16](
      Generators.array_of[U8](Generators.u8() where min = 1, max = 64),
      Generators.u16())

  fun ref property(arg: (Array[U8], U16), ph: PropertyHelper) ? =>
    (let data, let mask) = arg
    let data_val = _BH.ref_to_val(data)
    let rb = Reader
    for c in _BH.split_chunks(data_val, mask).values() do rb.append(c) end
    let result = rb.block(data_val.size())?
    ph.assert_eq[USize](result.size(), data_val.size())
    var i: USize = 0
    while i < data_val.size() do
      ph.assert_eq[U8](result(i)?, data_val(i)?)
      i = i + 1
    end

class \nodoc\ iso _PropSkipRead is Property1[(Array[U8], U8)]
  fun name(): String => "buffered/PropSkipRead"

  fun gen(): Generator[(Array[U8], U8)] =>
    Generators.zip2[Array[U8], U8](
      Generators.array_of[U8](Generators.u8() where min = 1, max = 100),
      Generators.u8())

  fun ref property(arg: (Array[U8], U8), ph: PropertyHelper) ? =>
    (let data, let skip_raw) = arg
    let data_val = _BH.ref_to_val(data)
    let skip_amount = skip_raw.usize() % data_val.size()
    let remaining = data_val.size() - skip_amount

    let rb = Reader
    rb.append(data_val)

    rb.skip(skip_amount)?
    ph.assert_eq[USize](rb.size(), remaining)

    let result = rb.block(remaining)?
    var i: USize = 0
    while i < remaining do
      ph.assert_eq[U8](result(i)?, data_val(skip_amount + i)?)
      i = i + 1
    end

class \nodoc\ iso _PropLineRoundtrip is Property1[(String, Bool)]
  fun name(): String => "buffered/PropLineRoundtrip"

  fun gen(): Generator[(String, Bool)] =>
    Generators.zip2[String, Bool](
      Generators.ascii(where min = 0, max = 50),
      Generators.bool())

  fun ref property(arg: (String, Bool), ph: PropertyHelper) ? =>
    (let content, let use_crlf) = arg
    let content_val: String val = content.clone()
    let clean =
      recover val
        let s = String(content_val.size())
        for byte in content_val.values() do
          if (byte != '\n') and (byte != '\r') then s.push(byte) end
        end
        s
      end

    let terminated =
      recover val
        let s = String(clean.size() + 2)
        s.append(clean)
        if use_crlf then s.append("\r\n") else s.push('\n') end
        s
      end

    var rb = Reader
    rb.append(terminated)
    ph.assert_eq[String](rb.line()?, clean)

    rb = Reader
    rb.append(terminated)
    ph.assert_eq[String](rb.line(true)?, terminated)

class \nodoc\ iso _PropReadUntil is Property1[(Array[U8], U8)]
  fun name(): String => "buffered/PropReadUntil"

  fun gen(): Generator[(Array[U8], U8)] =>
    Generators.zip2[Array[U8], U8](
      Generators.array_of[U8](Generators.u8() where min = 0, max = 50),
      Generators.u8())

  fun ref property(arg: (Array[U8], U8), ph: PropertyHelper) ? =>
    (let data, let sep) = arg
    let data_val = _BH.ref_to_val(data)
    let prefix =
      recover val
        let a = Array[U8]
        for byte in data_val.values() do
          if byte != sep then a.push(byte) end
        end
        a
      end

    let rb = Reader
    rb.append(prefix)
    rb.append(recover val [sep] end)

    let result = rb.read_until(sep)?
    ph.assert_eq[USize](result.size(), prefix.size())
    var i: USize = 0
    while i < prefix.size() do
      ph.assert_eq[U8](result(i)?, prefix(i)?)
      i = i + 1
    end
    ph.assert_eq[USize](rb.size(), 0)

class \nodoc\ iso _PropWriterDoneReset is Property1[(U64, U64)]
  fun name(): String => "buffered/PropWriterDoneReset"

  fun gen(): Generator[(U64, U64)] =>
    Generators.zip2[U64, U64](Generators.u64(), Generators.u64())

  fun ref property(arg: (U64, U64), ph: PropertyHelper) ? =>
    (let v1, let v2) = arg
    let wb = Writer
    wb.u64_be(v1)
    ph.assert_eq[USize](wb.size(), 8)
    let first: Array[ByteSeq] val = wb.done()
    ph.assert_eq[USize](wb.size(), 0)

    wb.u64_be(v2)
    ph.assert_eq[USize](wb.size(), 8)
    let second: Array[ByteSeq] val = wb.done()

    let rb1 = Reader
    for chunk in first.values() do rb1.append(chunk) end
    ph.assert_eq[U64](rb1.u64_be()?, v1)

    let rb2 = Reader
    for chunk in second.values() do rb2.append(chunk) end
    ph.assert_eq[U64](rb2.u64_be()?, v2)

class \nodoc\ iso _PropEndianCrossCheckU16 is Property1[U16]
  """
  BE and LE byte sequences are the reverse of each other for U16.
  """
  fun name(): String => "buffered/PropEndianCrossCheckU16"

  fun gen(): Generator[U16] =>
    Generators.frequency[U16](
      [ as WeightedGenerator[U16]:
        (8, Generators.u16())
        (1, Generators.one_of[U16](
          [as U16: 0; 1; 0xFF; 0x100; 0x7FFF; 0x8000; 0xFFFF]))
      ])

  fun ref property(v: U16, ph: PropertyHelper) ? =>
    var wb = Writer
    wb.u16_be(v)
    let bytes_be = _BH.writer_bytes(wb.done())

    wb = Writer
    wb.u16_le(v)
    let bytes_le = _BH.writer_bytes(wb.done())

    ph.assert_eq[USize](bytes_be.size(), 2)
    ph.assert_eq[USize](bytes_le.size(), 2)
    var i: USize = 0
    while i < 2 do
      ph.assert_eq[U8](bytes_be(i)?, bytes_le(1 - i)?)
      i = i + 1
    end

class \nodoc\ iso _PropEndianCrossCheckU32 is Property1[U32]
  """
  BE and LE byte sequences are the reverse of each other for U32.
  """
  fun name(): String => "buffered/PropEndianCrossCheckU32"

  fun gen(): Generator[U32] =>
    Generators.frequency[U32](
      [ as WeightedGenerator[U32]:
        (8, Generators.u32())
        (1, Generators.one_of[U32](
          [as U32: 0; 1; 0x01020304; 0xDEADBEEF; 0xFFFFFFFF]))
      ])

  fun ref property(v: U32, ph: PropertyHelper) ? =>
    var wb = Writer
    wb.u32_be(v)
    let bytes_be = _BH.writer_bytes(wb.done())

    wb = Writer
    wb.u32_le(v)
    let bytes_le = _BH.writer_bytes(wb.done())

    ph.assert_eq[USize](bytes_be.size(), 4)
    ph.assert_eq[USize](bytes_le.size(), 4)
    var i: USize = 0
    while i < 4 do
      ph.assert_eq[U8](bytes_be(i)?, bytes_le(3 - i)?)
      i = i + 1
    end

class \nodoc\ iso _PropEndianCrossCheckU64 is Property1[U64]
  """
  BE and LE byte sequences are the reverse of each other for U64.
  """
  fun name(): String => "buffered/PropEndianCrossCheckU64"

  fun gen(): Generator[U64] =>
    Generators.frequency[U64](
      [ as WeightedGenerator[U64]:
        (8, Generators.u64())
        (1, Generators.one_of[U64](
          [ as U64: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
            0xFFFFFFFF; 0x100000000; U64.max_value()]))
      ])

  fun ref property(v: U64, ph: PropertyHelper) ? =>
    var wb = Writer
    wb.u64_be(v)
    let bytes_be = _BH.writer_bytes(wb.done())

    wb = Writer
    wb.u64_le(v)
    let bytes_le = _BH.writer_bytes(wb.done())

    ph.assert_eq[USize](bytes_be.size(), 8)
    ph.assert_eq[USize](bytes_le.size(), 8)
    var i: USize = 0
    while i < 8 do
      ph.assert_eq[U8](bytes_be(i)?, bytes_le(7 - i)?)
      i = i + 1
    end

class \nodoc\ iso _PropEndianCrossCheckU128 is Property1[U128]
  """
  BE and LE byte sequences are the reverse of each other for U128.
  """
  fun name(): String => "buffered/PropEndianCrossCheckU128"

  fun gen(): Generator[U128] =>
    Generators.frequency[U128](
      [ as WeightedGenerator[U128]:
        (8, Generators.u128())
        (1, Generators.one_of[U128](
          [ as U128: 0; 1; 0xFF; 0x100; 0xFFFF; 0x10000
            0xFFFFFFFF; 0x100000000; 0xFFFFFFFFFFFFFFFF
            U128.max_value()]))
      ])

  fun ref property(v: U128, ph: PropertyHelper) ? =>
    var wb = Writer
    wb.u128_be(v)
    let bytes_be = _BH.writer_bytes(wb.done())

    wb = Writer
    wb.u128_le(v)
    let bytes_le = _BH.writer_bytes(wb.done())

    ph.assert_eq[USize](bytes_be.size(), 16)
    ph.assert_eq[USize](bytes_le.size(), 16)
    var i: USize = 0
    while i < 16 do
      ph.assert_eq[U8](bytes_be(i)?, bytes_le(15 - i)?)
      i = i + 1
    end

class \nodoc\ iso _PropEndianCrossCheckF32 is Property1[U32]
  """
  BE and LE byte sequences are the reverse of each other for F32.
  """
  fun name(): String => "buffered/PropEndianCrossCheckF32"

  fun gen(): Generator[U32] =>
    Generators.frequency[U32](
      [ as WeightedGenerator[U32]:
        (8, Generators.u32())
        (1, Generators.one_of[U32](
          [ as U32: 0; 0x80000000; 0x7F800000; 0xFF800000
            0x7FC00000; 0x3F800000; 0xFFFFFFFF]))
      ])

  fun ref property(bits: U32, ph: PropertyHelper) ? =>
    let v = F32.from_bits(bits)
    var wb = Writer
    wb.f32_be(v)
    let bytes_be = _BH.writer_bytes(wb.done())

    wb = Writer
    wb.f32_le(v)
    let bytes_le = _BH.writer_bytes(wb.done())

    ph.assert_eq[USize](bytes_be.size(), 4)
    ph.assert_eq[USize](bytes_le.size(), 4)
    var i: USize = 0
    while i < 4 do
      ph.assert_eq[U8](bytes_be(i)?, bytes_le(3 - i)?)
      i = i + 1
    end

class \nodoc\ iso _PropEndianCrossCheckF64 is Property1[U64]
  """
  BE and LE byte sequences are the reverse of each other for F64.
  """
  fun name(): String => "buffered/PropEndianCrossCheckF64"

  fun gen(): Generator[U64] =>
    Generators.frequency[U64](
      [ as WeightedGenerator[U64]:
        (8, Generators.u64())
        (1, Generators.one_of[U64](
          [ as U64: 0; 0x8000000000000000; 0x7FF0000000000000
            0xFFF0000000000000; 0x7FF8000000000000
            0x3FF0000000000000; U64.max_value()]))
      ])

  fun ref property(bits: U64, ph: PropertyHelper) ? =>
    let v = F64.from_bits(bits)
    var wb = Writer
    wb.f64_be(v)
    let bytes_be = _BH.writer_bytes(wb.done())

    wb = Writer
    wb.f64_le(v)
    let bytes_le = _BH.writer_bytes(wb.done())

    ph.assert_eq[USize](bytes_be.size(), 8)
    ph.assert_eq[USize](bytes_le.size(), 8)
    var i: USize = 0
    while i < 8 do
      ph.assert_eq[U8](bytes_be(i)?, bytes_le(7 - i)?)
      i = i + 1
    end

class \nodoc\ iso _PropMultiValueRoundtrip
  is Property1[((U8, U16), (U32, U64))]
  """
  Sequential writes of different types, read back in order, with offset
  tracking across multiple reads.
  """
  fun name(): String => "buffered/PropMultiValueRoundtrip"

  fun gen(): Generator[((U8, U16), (U32, U64))] =>
    Generators.zip2[(U8, U16), (U32, U64)](
      Generators.zip2[U8, U16](Generators.u8(), Generators.u16()),
      Generators.zip2[U32, U64](Generators.u32(), Generators.u64()))

  fun ref property(arg: ((U8, U16), (U32, U64)), ph: PropertyHelper) ? =>
    ((let a, let b), (let c, let d)) = arg
    let wb = Writer
    wb.u8(a)
    wb.u16_be(b)
    wb.u32_le(c)
    wb.u64_be(d)
    ph.assert_eq[USize](wb.size(), 1 + 2 + 4 + 8)

    let rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end

    ph.assert_eq[U8](rb.u8()?, a)
    ph.assert_eq[U16](rb.u16_be()?, b)
    ph.assert_eq[U32](rb.u32_le()?, c)
    ph.assert_eq[U64](rb.u64_be()?, d)
    ph.assert_eq[USize](rb.size(), 0)

class \nodoc\ iso _PropWriterSize is Property1[Array[U8]]
  """
  Writer.size() with individual u8() writes and write() above the
  coalescing threshold.
  """
  fun name(): String => "buffered/PropWriterSize"

  fun gen(): Generator[Array[U8]] =>
    Generators.array_of[U8](Generators.u8() where min = 0, max = 200)

  fun ref property(data: Array[U8], ph: PropertyHelper) ? =>
    let data_val = _BH.ref_to_val(data)
    var wb = Writer
    for byte in data_val.values() do wb.u8(byte) end
    ph.assert_eq[USize](wb.size(), data_val.size())

    wb = Writer
    wb.write(data_val)
    ph.assert_eq[USize](wb.size(), data_val.size())

    let rb = Reader
    for chunk in (wb.done()).values() do rb.append(chunk) end
    ph.assert_eq[USize](rb.size(), data_val.size())
    if data_val.size() > 0 then
      let result = rb.block(data_val.size())?
      var i: USize = 0
      while i < data_val.size() do
        ph.assert_eq[U8](result(i)?, data_val(i)?)
        i = i + 1
      end
    end

class \nodoc\ iso _PropEmptyAppend is Property1[Array[U8]]
  """
  Empty appends do not corrupt the Reader's chunk list.
  """
  fun name(): String => "buffered/PropEmptyAppend"

  fun gen(): Generator[Array[U8]] =>
    Generators.array_of[U8](Generators.u8() where min = 1, max = 50)

  fun ref property(data: Array[U8], ph: PropertyHelper) ? =>
    let data_val = _BH.ref_to_val(data)
    let rb = Reader
    rb.append(recover val Array[U8] end)
    rb.append("")
    rb.append(data_val)
    rb.append(recover val Array[U8] end)
    rb.append("")
    ph.assert_eq[USize](rb.size(), data_val.size())
    let result = rb.block(data_val.size())?
    var i: USize = 0
    while i < data_val.size() do
      ph.assert_eq[U8](result(i)?, data_val(i)?)
      i = i + 1
    end
    ph.assert_eq[USize](rb.size(), 0)
