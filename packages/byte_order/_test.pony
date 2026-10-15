use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Property-based: unsigned roundtrip
    test.property[U16](_PropertyBERoundtripU16)
    test.property[U16](_PropertyLERoundtripU16)
    test.property[U32](_PropertyBERoundtripU32)
    test.property[U32](_PropertyLERoundtripU32)
    test.property[U64](_PropertyBERoundtripU64)
    test.property[U64](_PropertyLERoundtripU64)
    test.property[U128](_PropertyBERoundtripU128)
    test.property[U128](_PropertyLERoundtripU128)
    // Property-based: endian cross-check
    test.property[U16](_PropertyEndianCrossCheckU16)
    test.property[U32](_PropertyEndianCrossCheckU32)
    test.property[U64](_PropertyEndianCrossCheckU64)
    test.property[U128](_PropertyEndianCrossCheckU128)
    // Property-based: ReadSeq vs Array consistency
    test.property[U16](_PropertyReadSeqConsistencyU16)
    test.property[U32](_PropertyReadSeqConsistencyU32)
    test.property[U64](_PropertyReadSeqConsistencyU64)
    test.property[U128](_PropertyReadSeqConsistencyU128)
    // Property-based: signed roundtrip
    test.property[U16](_PropertySignedRoundtripI16)
    test.property[U32](_PropertySignedRoundtripI32)
    test.property[U64](_PropertySignedRoundtripI64)
    test.property[U128](_PropertySignedRoundtripI128)
    // Property-based: float roundtrip
    test.property[U32](_PropertyFloatRoundtripF32)
    test.property[U64](_PropertyFloatRoundtripF64)
    // Unit tests
    test(_TestKnownValueBE)
    test(_TestKnownValueLE)
    test(_TestOffsetRead)
    test(_TestOffsetReadGeneric)
    test(_TestShortDataError)
    test(_TestShortDataErrorGeneric)

// A ReadSeq wrapper that delegates byte-by-byte, preventing the Array[U8]
// iftype specialization from matching. This lets us verify that the
// shift-and-OR default path agrees with the optimized Array path.
class \nodoc\ val _ByteSeq is ReadSeq[U8]
  let _data: Array[U8] val

  new val create(data: Array[U8] val) =>
    _data = data

  fun size(): USize => _data.size()
  fun apply(i: USize): U8 ? => _data(i)?
  fun values(): Iterator[U8]^ => _data.values()

class \nodoc\ iso _PropertyBERoundtripU16 is Property[U16]
  fun name(): String => "byte_order/BigEndian.roundtrip_u16"
  fun gen(): Generator[U16] => Generators.u16()

  fun property(sample: U16, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u16(buf, sample)
    h.assert_eq[U16](sample, BigEndian.u16[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyLERoundtripU16 is Property[U16]
  fun name(): String => "byte_order/LittleEndian.roundtrip_u16"
  fun gen(): Generator[U16] => Generators.u16()

  fun property(sample: U16, h: PropertyHelper) ? =>
    let buf = Array[U8]
    LittleEndian.push_u16(buf, sample)
    h.assert_eq[U16](sample, LittleEndian.u16[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyBERoundtripU32 is Property[U32]
  fun name(): String => "byte_order/BigEndian.roundtrip_u32"
  fun gen(): Generator[U32] => Generators.u32()

  fun property(sample: U32, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u32(buf, sample)
    h.assert_eq[U32](sample, BigEndian.u32[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyLERoundtripU32 is Property[U32]
  fun name(): String => "byte_order/LittleEndian.roundtrip_u32"
  fun gen(): Generator[U32] => Generators.u32()

  fun property(sample: U32, h: PropertyHelper) ? =>
    let buf = Array[U8]
    LittleEndian.push_u32(buf, sample)
    h.assert_eq[U32](sample, LittleEndian.u32[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyBERoundtripU64 is Property[U64]
  fun name(): String => "byte_order/BigEndian.roundtrip_u64"
  fun gen(): Generator[U64] => Generators.u64()

  fun property(sample: U64, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u64(buf, sample)
    h.assert_eq[U64](sample, BigEndian.u64[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyLERoundtripU64 is Property[U64]
  fun name(): String => "byte_order/LittleEndian.roundtrip_u64"
  fun gen(): Generator[U64] => Generators.u64()

  fun property(sample: U64, h: PropertyHelper) ? =>
    let buf = Array[U8]
    LittleEndian.push_u64(buf, sample)
    h.assert_eq[U64](sample, LittleEndian.u64[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyBERoundtripU128 is Property[U128]
  fun name(): String => "byte_order/BigEndian.roundtrip_u128"
  fun gen(): Generator[U128] => Generators.u128()

  fun property(sample: U128, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u128(buf, sample)
    h.assert_eq[U128](sample, BigEndian.u128[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyLERoundtripU128 is Property[U128]
  fun name(): String => "byte_order/LittleEndian.roundtrip_u128"
  fun gen(): Generator[U128] => Generators.u128()

  fun property(sample: U128, h: PropertyHelper) ? =>
    let buf = Array[U8]
    LittleEndian.push_u128(buf, sample)
    h.assert_eq[U128](sample, LittleEndian.u128[Array[U8] box](buf)?)

class \nodoc\ iso _PropertyEndianCrossCheckU16 is Property[U16]
  fun name(): String => "byte_order/endian_cross_check_u16"
  fun gen(): Generator[U16] => Generators.u16()

  fun property(sample: U16, h: PropertyHelper) =>
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_u16(be_buf, sample)
    LittleEndian.push_u16(le_buf, sample)
    h.assert_eq[USize](2, be_buf.size())
    h.assert_eq[USize](2, le_buf.size())
    try
      h.assert_eq[U8](be_buf(0)?, le_buf(1)?)
      h.assert_eq[U8](be_buf(1)?, le_buf(0)?)
    else
      h.fail("unexpected indexing error")
    end

class \nodoc\ iso _PropertyEndianCrossCheckU32 is Property[U32]
  fun name(): String => "byte_order/endian_cross_check_u32"
  fun gen(): Generator[U32] => Generators.u32()

  fun property(sample: U32, h: PropertyHelper) =>
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_u32(be_buf, sample)
    LittleEndian.push_u32(le_buf, sample)
    h.assert_eq[USize](4, be_buf.size())
    h.assert_eq[USize](4, le_buf.size())
    try
      var i: USize = 0
      while i < 4 do
        h.assert_eq[U8](be_buf(i)?, le_buf(3 - i)?)
        i = i + 1
      end
    else
      h.fail("unexpected indexing error")
    end

class \nodoc\ iso _PropertyEndianCrossCheckU64 is Property[U64]
  fun name(): String => "byte_order/endian_cross_check_u64"
  fun gen(): Generator[U64] => Generators.u64()

  fun property(sample: U64, h: PropertyHelper) =>
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_u64(be_buf, sample)
    LittleEndian.push_u64(le_buf, sample)
    h.assert_eq[USize](8, be_buf.size())
    h.assert_eq[USize](8, le_buf.size())
    try
      var i: USize = 0
      while i < 8 do
        h.assert_eq[U8](be_buf(i)?, le_buf(7 - i)?)
        i = i + 1
      end
    else
      h.fail("unexpected indexing error")
    end

class \nodoc\ iso _PropertyEndianCrossCheckU128 is Property[U128]
  fun name(): String => "byte_order/endian_cross_check_u128"
  fun gen(): Generator[U128] => Generators.u128()

  fun property(sample: U128, h: PropertyHelper) =>
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_u128(be_buf, sample)
    LittleEndian.push_u128(le_buf, sample)
    h.assert_eq[USize](16, be_buf.size())
    h.assert_eq[USize](16, le_buf.size())
    try
      var i: USize = 0
      while i < 16 do
        h.assert_eq[U8](be_buf(i)?, le_buf(15 - i)?)
        i = i + 1
      end
    else
      h.fail("unexpected indexing error")
    end

class \nodoc\ iso _PropertyReadSeqConsistencyU16 is Property[U16]
  fun name(): String => "byte_order/readseq_vs_array_u16"
  fun gen(): Generator[U16] => Generators.u16()

  fun property(sample: U16, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u16(buf, sample)
    let arr_val: Array[U8] val = (consume buf).clone()
    let seq = _ByteSeq(arr_val)
    h.assert_eq[U16](
      BigEndian.u16[Array[U8] val](arr_val)?,
      BigEndian.u16[_ByteSeq](seq)?)
    h.assert_eq[U16](
      LittleEndian.u16[Array[U8] val](arr_val)?,
      LittleEndian.u16[_ByteSeq](seq)?)

class \nodoc\ iso _PropertyReadSeqConsistencyU32 is Property[U32]
  fun name(): String => "byte_order/readseq_vs_array_u32"
  fun gen(): Generator[U32] => Generators.u32()

  fun property(sample: U32, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u32(buf, sample)
    let arr_val: Array[U8] val = (consume buf).clone()
    let seq = _ByteSeq(arr_val)
    h.assert_eq[U32](
      BigEndian.u32[Array[U8] val](arr_val)?,
      BigEndian.u32[_ByteSeq](seq)?)
    h.assert_eq[U32](
      LittleEndian.u32[Array[U8] val](arr_val)?,
      LittleEndian.u32[_ByteSeq](seq)?)

class \nodoc\ iso _PropertyReadSeqConsistencyU64 is Property[U64]
  fun name(): String => "byte_order/readseq_vs_array_u64"
  fun gen(): Generator[U64] => Generators.u64()

  fun property(sample: U64, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u64(buf, sample)
    let arr_val: Array[U8] val = (consume buf).clone()
    let seq = _ByteSeq(arr_val)
    h.assert_eq[U64](
      BigEndian.u64[Array[U8] val](arr_val)?,
      BigEndian.u64[_ByteSeq](seq)?)
    h.assert_eq[U64](
      LittleEndian.u64[Array[U8] val](arr_val)?,
      LittleEndian.u64[_ByteSeq](seq)?)

class \nodoc\ iso _PropertyReadSeqConsistencyU128 is Property[U128]
  fun name(): String => "byte_order/readseq_vs_array_u128"
  fun gen(): Generator[U128] => Generators.u128()

  fun property(sample: U128, h: PropertyHelper) ? =>
    let buf = Array[U8]
    BigEndian.push_u128(buf, sample)
    let arr_val: Array[U8] val = (consume buf).clone()
    let seq = _ByteSeq(arr_val)
    h.assert_eq[U128](
      BigEndian.u128[Array[U8] val](arr_val)?,
      BigEndian.u128[_ByteSeq](seq)?)
    h.assert_eq[U128](
      LittleEndian.u128[Array[U8] val](arr_val)?,
      LittleEndian.u128[_ByteSeq](seq)?)

class \nodoc\ iso _PropertySignedRoundtripI16 is Property[U16]
  fun name(): String => "byte_order/signed_roundtrip_i16"
  fun gen(): Generator[U16] => Generators.u16()

  fun property(sample: U16, h: PropertyHelper) ? =>
    let v = sample.i16()
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_i16(be_buf, v)
    LittleEndian.push_i16(le_buf, v)
    h.assert_eq[I16](v, BigEndian.i16[Array[U8] box](be_buf)?)
    h.assert_eq[I16](v, LittleEndian.i16[Array[U8] box](le_buf)?)

class \nodoc\ iso _PropertySignedRoundtripI32 is Property[U32]
  fun name(): String => "byte_order/signed_roundtrip_i32"
  fun gen(): Generator[U32] => Generators.u32()

  fun property(sample: U32, h: PropertyHelper) ? =>
    let v = sample.i32()
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_i32(be_buf, v)
    LittleEndian.push_i32(le_buf, v)
    h.assert_eq[I32](v, BigEndian.i32[Array[U8] box](be_buf)?)
    h.assert_eq[I32](v, LittleEndian.i32[Array[U8] box](le_buf)?)

class \nodoc\ iso _PropertySignedRoundtripI64 is Property[U64]
  fun name(): String => "byte_order/signed_roundtrip_i64"
  fun gen(): Generator[U64] => Generators.u64()

  fun property(sample: U64, h: PropertyHelper) ? =>
    let v = sample.i64()
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_i64(be_buf, v)
    LittleEndian.push_i64(le_buf, v)
    h.assert_eq[I64](v, BigEndian.i64[Array[U8] box](be_buf)?)
    h.assert_eq[I64](v, LittleEndian.i64[Array[U8] box](le_buf)?)

class \nodoc\ iso _PropertySignedRoundtripI128 is Property[U128]
  fun name(): String => "byte_order/signed_roundtrip_i128"
  fun gen(): Generator[U128] => Generators.u128()

  fun property(sample: U128, h: PropertyHelper) ? =>
    let v = sample.i128()
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_i128(be_buf, v)
    LittleEndian.push_i128(le_buf, v)
    h.assert_eq[I128](v, BigEndian.i128[Array[U8] box](be_buf)?)
    h.assert_eq[I128](v, LittleEndian.i128[Array[U8] box](le_buf)?)

class \nodoc\ iso _PropertyFloatRoundtripF32 is Property[U32]
  fun name(): String => "byte_order/float_roundtrip_f32"
  fun gen(): Generator[U32] => Generators.u32()

  fun property(sample: U32, h: PropertyHelper) ? =>
    let v = F32.from_bits(sample)
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_f32(be_buf, v)
    LittleEndian.push_f32(le_buf, v)
    h.assert_eq[U32](
      sample, BigEndian.f32[Array[U8] box](be_buf)?.bits())
    h.assert_eq[U32](
      sample, LittleEndian.f32[Array[U8] box](le_buf)?.bits())

class \nodoc\ iso _PropertyFloatRoundtripF64 is Property[U64]
  fun name(): String => "byte_order/float_roundtrip_f64"
  fun gen(): Generator[U64] => Generators.u64()

  fun property(sample: U64, h: PropertyHelper) ? =>
    let v = F64.from_bits(sample)
    let be_buf = Array[U8]
    let le_buf = Array[U8]
    BigEndian.push_f64(be_buf, v)
    LittleEndian.push_f64(le_buf, v)
    h.assert_eq[U64](
      sample, BigEndian.f64[Array[U8] box](be_buf)?.bits())
    h.assert_eq[U64](
      sample, LittleEndian.f64[Array[U8] box](le_buf)?.bits())

class \nodoc\ iso _TestKnownValueBE is UnitTest
  fun name(): String => "byte_order/BigEndian.known_value"

  fun apply(h: TestHelper) ? =>
    let data: Array[U8] val = [0xDE; 0xAD; 0xBE; 0xEF]
    h.assert_eq[U32](0xDEADBEEF, BigEndian.u32[Array[U8] val](data)?)
    h.assert_eq[U16](0xDEAD, BigEndian.u16[Array[U8] val](data)?)
    h.assert_eq[U16](0xBEEF, BigEndian.u16[Array[U8] val](data, 2)?)

class \nodoc\ iso _TestKnownValueLE is UnitTest
  fun name(): String => "byte_order/LittleEndian.known_value"

  fun apply(h: TestHelper) ? =>
    let data: Array[U8] val = [0xEF; 0xBE; 0xAD; 0xDE]
    h.assert_eq[U32](0xDEADBEEF, LittleEndian.u32[Array[U8] val](data)?)
    h.assert_eq[U16](0xBEEF, LittleEndian.u16[Array[U8] val](data)?)
    h.assert_eq[U16](0xDEAD, LittleEndian.u16[Array[U8] val](data, 2)?)

class \nodoc\ iso _TestOffsetRead is UnitTest
  fun name(): String => "byte_order/offset_read"

  fun apply(h: TestHelper) ? =>
    let data: Array[U8] val =
      [0x00; 0x01; 0x02; 0x03; 0x04; 0x05; 0x06; 0x07]
    h.assert_eq[U16](0x0001, BigEndian.u16[Array[U8] val](data, 0)?)
    h.assert_eq[U16](0x0203, BigEndian.u16[Array[U8] val](data, 2)?)
    h.assert_eq[U16](0x0607, BigEndian.u16[Array[U8] val](data, 6)?)
    h.assert_eq[U32](0x02030405, BigEndian.u32[Array[U8] val](data, 2)?)
    h.assert_eq[U64](
      0x0001020304050607, BigEndian.u64[Array[U8] val](data)?)

    h.assert_eq[U16](0x0100, LittleEndian.u16[Array[U8] val](data, 0)?)
    h.assert_eq[U16](0x0302, LittleEndian.u16[Array[U8] val](data, 2)?)
    h.assert_eq[U32](0x05040302, LittleEndian.u32[Array[U8] val](data, 2)?)

class \nodoc\ iso _TestShortDataError is UnitTest
  fun name(): String => "byte_order/short_data_error"

  fun apply(h: TestHelper) =>
    let one_byte: Array[U8] val = [0xFF]
    let three_bytes: Array[U8] val = [0xFF; 0xFF; 0xFF]
    let seven_bytes: Array[U8] val =
      [0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF]

    h.assert_error({()? => BigEndian.u16[Array[U8] val](one_byte)? })
    h.assert_error({()? => BigEndian.u32[Array[U8] val](three_bytes)? })
    h.assert_error({()? => BigEndian.u64[Array[U8] val](seven_bytes)? })
    h.assert_error({()? =>
      BigEndian.u128[Array[U8] val](seven_bytes)?
    })

    h.assert_error({()? => LittleEndian.u16[Array[U8] val](one_byte)? })
    h.assert_error({()? => LittleEndian.u32[Array[U8] val](three_bytes)? })
    h.assert_error({()? => LittleEndian.u64[Array[U8] val](seven_bytes)? })
    h.assert_error({()? =>
      LittleEndian.u128[Array[U8] val](seven_bytes)?
    })

    // Offset past end
    let four_bytes: Array[U8] val = [0xFF; 0xFF; 0xFF; 0xFF]
    h.assert_error({()? =>
      BigEndian.u32[Array[U8] val](four_bytes, 1)?
    })
    h.assert_error({()? =>
      LittleEndian.u32[Array[U8] val](four_bytes, 1)?
    })

class \nodoc\ iso _TestOffsetReadGeneric is UnitTest
  fun name(): String => "byte_order/offset_read_generic"

  fun apply(h: TestHelper) ? =>
    let data: Array[U8] val =
      [0x00; 0x01; 0x02; 0x03; 0x04; 0x05; 0x06; 0x07]
    let seq = _ByteSeq(data)
    h.assert_eq[U16](0x0001, BigEndian.u16[_ByteSeq](seq, 0)?)
    h.assert_eq[U16](0x0203, BigEndian.u16[_ByteSeq](seq, 2)?)
    h.assert_eq[U32](0x02030405, BigEndian.u32[_ByteSeq](seq, 2)?)

    h.assert_eq[U16](0x0100, LittleEndian.u16[_ByteSeq](seq, 0)?)
    h.assert_eq[U16](0x0302, LittleEndian.u16[_ByteSeq](seq, 2)?)
    h.assert_eq[U32](0x05040302, LittleEndian.u32[_ByteSeq](seq, 2)?)

class \nodoc\ iso _TestShortDataErrorGeneric is UnitTest
  fun name(): String => "byte_order/short_data_error_generic"

  fun apply(h: TestHelper) =>
    let one_byte = _ByteSeq([0xFF])
    let three_bytes = _ByteSeq([0xFF; 0xFF; 0xFF])

    h.assert_error({()? => BigEndian.u16[_ByteSeq](one_byte)? })
    h.assert_error({()? => BigEndian.u32[_ByteSeq](three_bytes)? })
    h.assert_error({()? => LittleEndian.u16[_ByteSeq](one_byte)? })
    h.assert_error({()? => LittleEndian.u32[_ByteSeq](three_bytes)? })

    let four_bytes = _ByteSeq([0xFF; 0xFF; 0xFF; 0xFF])
    h.assert_error({()? =>
      BigEndian.u32[_ByteSeq](four_bytes, 1)?
    })
    h.assert_error({()? =>
      LittleEndian.u32[_ByteSeq](four_bytes, 1)?
    })
