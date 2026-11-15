use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestU8Codec)
    test(_TestU16BE)
    test(_TestU16LE)
    test(_TestU32BE)
    test(_TestU32LE)
    test(_TestU64BE)
    test(_TestU64LE)
    test(_TestI8Codec)
    test(_TestI16BE)
    test(_TestI32BE)
    test(_TestI64BE)
    test(_TestU128BE)
    test(_TestU128LE)
    test(_TestI128BE)
    test(_TestF32BE)
    test(_TestF64BE)
    test(_TestBoolBit)
    test(_TestBitsU8)
    test(_TestBitsU16)
    test(_TestBytes)
    test(_TestBytesRest)
    test(_TestLiteral)
    test(_TestSeq2)
    test(_TestSeq3)
    test(_TestSeq4)
    test(_TestMapped)
    test(_TestFlatZip)
    test(_TestThenConst)
    test(_TestConstThen)
    test(_TestMany)
    test(_TestCount)
    test(_TestCountRoundtrip)
    test(_TestCountOverflow)
    test(_TestCountU32)
    test(_TestCountDecodeOverflow)
    test(_TestConditional)
    test(_TestSkipBits)
    test(_TestVariableSize)
    test(_TestVariableSizeRoundtrip)
    test(_TestVariableSizeOverflow)
    test(_TestVariableSizeU32)
    test(_TestVariableSizeDecodeOverflow)
    test(_TestFrameDecoder)
    test(_TestFrameDecoderSubByte)
    test(_TestFrameDecoderNonAligned)
    test(_TestFrameDecoderSubByteRemainder)
    test(_TestFrameDecoderOddBits)
    test(_TestFrameDecoderMalformedRecovery)
    test(_TestFrameDecoderMalformedAfterGood)
    test(_TestFrameDecoderMalformedConsecutive)
    test(_TestSubByteBitFields)
    test(_TestRoundtripU8)
    test(_TestRoundtripU16BE)
    test(_TestRoundtripSeq2)
    test(_TestRoundtripBits)
    test(_TestUnalignedU8Codec)
    test(_TestUnalignedI8Codec)
    test(_TestUnalignedU16LE)
    test(_TestUnalignedU32LE)
    test(_TestUnalignedU64LE)
    test(_TestUnalignedU128BE)
    test(_TestUnalignedU128LE)
    test(_TestUnalignedBytes)
    test(_TestUnalignedBytesRest)
    test(_TestUnalignedLiteral)
    test(_TestUnalignedVariableSize)
    test(_TestDiscriminated)

class \nodoc\ iso _TestU8Codec is UnitTest
  fun name(): String => "binary_codec/U8Codec"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x42]
    match U8Codec.decode(data, 0)
    | (let v: U8, let c: USize) =>
      h.assert_eq[U8](0x42, v)
      h.assert_eq[USize](8, c)
    else
      h.fail("decode failed")
    end

    match U8Codec.decode(data, 8)
    | InsufficientData => None
    else
      h.fail("expected InsufficientData")
    end

class \nodoc\ iso _TestU16BE is UnitTest
  fun name(): String => "binary_codec/U16BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x01; 0x02]
    match U16BE.decode(data, 0)
    | (let v: U16, let c: USize) =>
      h.assert_eq[U16](0x0102, v)
      h.assert_eq[USize](16, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU16LE is UnitTest
  fun name(): String => "binary_codec/U16LE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x01; 0x02]
    match U16LE.decode(data, 0)
    | (let v: U16, let c: USize) =>
      h.assert_eq[U16](0x0201, v)
      h.assert_eq[USize](16, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU32BE is UnitTest
  fun name(): String => "binary_codec/U32BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x00; 0x00; 0x01; 0x00]
    match U32BE.decode(data, 0)
    | (let v: U32, let c: USize) =>
      h.assert_eq[U32](256, v)
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU32LE is UnitTest
  fun name(): String => "binary_codec/U32LE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x01; 0x00; 0x00; 0x00]
    match U32LE.decode(data, 0)
    | (let v: U32, let c: USize) =>
      h.assert_eq[U32](1, v)
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU64BE is UnitTest
  fun name(): String => "binary_codec/U64BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val =
      [0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x01; 0x00]
    match U64BE.decode(data, 0)
    | (let v: U64, let c: USize) =>
      h.assert_eq[U64](256, v)
      h.assert_eq[USize](64, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU64LE is UnitTest
  fun name(): String => "binary_codec/U64LE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val =
      [0x01; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00]
    match U64LE.decode(data, 0)
    | (let v: U64, let c: USize) =>
      h.assert_eq[U64](1, v)
      h.assert_eq[USize](64, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestI8Codec is UnitTest
  fun name(): String => "binary_codec/I8Codec"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0xFF]
    match I8Codec.decode(data, 0)
    | (let v: I8, let c: USize) =>
      h.assert_eq[I8](-1, v)
      h.assert_eq[USize](8, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestI16BE is UnitTest
  fun name(): String => "binary_codec/I16BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0xFF; 0xFE]
    match I16BE.decode(data, 0)
    | (let v: I16, let c: USize) =>
      h.assert_eq[I16](-2, v)
      h.assert_eq[USize](16, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestI32BE is UnitTest
  fun name(): String => "binary_codec/I32BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0xFF; 0xFF; 0xFF; 0xFE]
    match I32BE.decode(data, 0)
    | (let v: I32, let c: USize) =>
      h.assert_eq[I32](-2, v)
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestI64BE is UnitTest
  fun name(): String => "binary_codec/I64BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val =
      [0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFE]
    match I64BE.decode(data, 0)
    | (let v: I64, let c: USize) =>
      h.assert_eq[I64](-2, v)
      h.assert_eq[USize](64, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU128BE is UnitTest
  fun name(): String => "binary_codec/U128BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val =
      [ 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00
        0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x01; 0x00 ]
    match U128BE.decode(data, 0)
    | (let v: U128, let c: USize) =>
      h.assert_eq[U128](256, v)
      h.assert_eq[USize](128, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestU128LE is UnitTest
  fun name(): String => "binary_codec/U128LE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val =
      [ 0x01; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00
        0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00 ]
    match U128LE.decode(data, 0)
    | (let v: U128, let c: USize) =>
      h.assert_eq[U128](1, v)
      h.assert_eq[USize](128, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestI128BE is UnitTest
  fun name(): String => "binary_codec/I128BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val =
      [ 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF
        0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFF; 0xFE ]
    match I128BE.decode(data, 0)
    | (let v: I128, let c: USize) =>
      h.assert_eq[I128](-2, v)
      h.assert_eq[USize](128, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestF32BE is UnitTest
  fun name(): String => "binary_codec/F32BE"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x40; 0x49; 0x0F; 0xDB]
    match F32BE.decode(data, 0)
    | (let v: F32, let c: USize) =>
      h.assert_true((v - 3.14159).abs() < 0.0001)
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestF64BE is UnitTest
  fun name(): String => "binary_codec/F64BE"

  fun apply(h: TestHelper) =>
    let pi: F64 = 3.141592653589793
    let bits = pi.bits()
    let data: Array[U8] val =
      [
        ((bits >> 56) and 0xFF).u8()
        ((bits >> 48) and 0xFF).u8()
        ((bits >> 40) and 0xFF).u8()
        ((bits >> 32) and 0xFF).u8()
        ((bits >> 24) and 0xFF).u8()
        ((bits >> 16) and 0xFF).u8()
        ((bits >> 8) and 0xFF).u8()
        (bits and 0xFF).u8()
      ]
    match F64BE.decode(data, 0)
    | (let v: F64, let c: USize) =>
      h.assert_eq[F64](pi, v)
      h.assert_eq[USize](64, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestBoolBit is UnitTest
  fun name(): String => "binary_codec/BoolBit"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x80]
    match BoolBit.decode(data, 0)
    | (let v: Bool, let c: USize) =>
      h.assert_true(v)
      h.assert_eq[USize](1, c)
    else
      h.fail("decode true failed")
    end

    match BoolBit.decode(data, 1)
    | (let v: Bool, let c: USize) =>
      h.assert_false(v)
      h.assert_eq[USize](1, c)
    else
      h.fail("decode false failed")
    end

class \nodoc\ iso _TestBitsU8 is UnitTest
  fun name(): String => "binary_codec/BitsU8"

  fun apply(h: TestHelper) =>
    // 0x45 = 0100 0101 => version=4, ihl=5
    let data: Array[U8] val = [0x45]
    let codec = BitsU8(4)
    match codec.decode(data, 0)
    | (let v: U8, let c: USize) =>
      h.assert_eq[U8](4, v)
      h.assert_eq[USize](4, c)
    else
      h.fail("first nibble decode failed")
    end
    match codec.decode(data, 4)
    | (let v: U8, let c: USize) =>
      h.assert_eq[U8](5, v)
      h.assert_eq[USize](4, c)
    else
      h.fail("second nibble decode failed")
    end

class \nodoc\ iso _TestBitsU16 is UnitTest
  fun name(): String => "binary_codec/BitsU16"

  fun apply(h: TestHelper) =>
    // 13-bit value: 0x1FFF = 8191
    // In bits: 0001 1111 1111 1111 plus 3 padding bits
    // Packed into 2 bytes: 0xFF F8 (shifted left 3)
    // Actually let's use a simpler test: 13 bits from 0xFF 0xF8
    // 1111 1111 1111 1000 -> first 13 bits = 1 1111 1111 1111 = 8191
    let data: Array[U8] val = [0xFF; 0xF8]
    let codec = BitsU16(13)
    match codec.decode(data, 0)
    | (let v: U16, let c: USize) =>
      h.assert_eq[U16](8191, v)
      h.assert_eq[USize](13, c)
    else
      h.fail("13-bit decode failed")
    end

class \nodoc\ iso _TestBytes is UnitTest
  fun name(): String => "binary_codec/Bytes"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x48; 0x65; 0x6C; 0x6C; 0x6F]
    let codec = Bytes(3)
    match codec.decode(data, 0)
    | (let v: Array[U8] val, let c: USize) =>
      h.assert_eq[USize](3, v.size())
      try
        h.assert_eq[U8](0x48, v(0)?)
        h.assert_eq[U8](0x65, v(1)?)
        h.assert_eq[U8](0x6C, v(2)?)
      else
        h.fail("byte access failed")
      end
      h.assert_eq[USize](24, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestBytesRest is UnitTest
  fun name(): String => "binary_codec/BytesRest"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x48; 0x65; 0x6C; 0x6C; 0x6F]
    match BytesRest.decode(data, 0)
    | (let v: Array[U8] val, let c: USize) =>
      h.assert_eq[USize](5, v.size())
      h.assert_eq[USize](40, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestLiteral is UnitTest
  fun name(): String => "binary_codec/Literal"

  fun apply(h: TestHelper) =>
    let magic: Array[U8] val = [0x89; 0x50; 0x4E; 0x47]
    let data: Array[U8] val = [0x89; 0x50; 0x4E; 0x47; 0xFF]
    let codec = Literal(magic)
    match codec.decode(data, 0)
    | (None, let c: USize) =>
      h.assert_eq[USize](32, c)
    else
      h.fail("match failed")
    end

    let bad: Array[U8] val = [0x89; 0x50; 0x00; 0x47]
    match codec.decode(bad, 0)
    | let m: MalformedData => None
    else
      h.fail("expected MalformedData on mismatch")
    end

class \nodoc\ iso _TestSeq2 is UnitTest
  fun name(): String => "binary_codec/Seq2"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x42; 0x00; 0x50]
    let codec = Seq2[U8, U16](U8Codec, U16BE)
    match codec.decode(data, 0)
    | ((let a: U8, let b: U16), let c: USize) =>
      h.assert_eq[U8](0x42, a)
      h.assert_eq[U16](0x0050, b)
      h.assert_eq[USize](24, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestSeq3 is UnitTest
  fun name(): String => "binary_codec/Seq3"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x01; 0x02; 0x03]
    let codec = Seq3[U8, U8, U8](U8Codec, U8Codec, U8Codec)
    match codec.decode(data, 0)
    | ((let a: U8, let b: U8, let c': U8), let c: USize) =>
      h.assert_eq[U8](1, a)
      h.assert_eq[U8](2, b)
      h.assert_eq[U8](3, c')
      h.assert_eq[USize](24, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestSeq4 is UnitTest
  fun name(): String => "binary_codec/Seq4"

  fun apply(h: TestHelper) =>
    // UDP header: 4 x U16BE
    let data: Array[U8] val =
      [0x00; 0x50; 0x00; 0x51; 0x00; 0x08; 0x12; 0x34]
    let codec = Seq4[U16, U16, U16, U16](U16BE, U16BE, U16BE, U16BE)
    match codec.decode(data, 0)
    | ((let a: U16, let b: U16, let c': U16, let d: U16), let c: USize) =>
      h.assert_eq[U16](80, a)
      h.assert_eq[U16](81, b)
      h.assert_eq[U16](8, c')
      h.assert_eq[U16](0x1234, d)
      h.assert_eq[USize](64, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestMapped is UnitTest
  fun name(): String => "binary_codec/Mapped"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x00; 0x50; 0x00; 0x51]
    let codec =
      Mapped[(U16, U16), _TestPair](
        Seq2[U16, U16](U16BE, U16BE),
        {(t: (U16, U16)): _TestPair => _TestPair(t._1, t._2) },
        {(p: _TestPair): (U16, U16) => (p.first, p.second) })
    match codec.decode(data, 0)
    | (let v: _TestPair, let c: USize) =>
      h.assert_eq[U16](80, v.first)
      h.assert_eq[U16](81, v.second)
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class val _TestPair
  let first: U16
  let second: U16

  new val create(f: U16, s: U16) => first = f; second = s

class \nodoc\ iso _TestFlatZip is UnitTest
  fun name(): String => "binary_codec/FlatZip"

  fun apply(h: TestHelper) =>
    // Length-prefixed: U8 length, then that many bytes
    let data: Array[U8] val = [0x03; 0x41; 0x42; 0x43]
    let codec =
      FlatZip[U8, Array[U8] val](
        U8Codec,
        {(len: U8): Codec[Array[U8] val] => Bytes(len.usize()) })
    match codec.decode(data, 0)
    | ((let a: U8, let b: Array[U8] val), let c: USize) =>
      h.assert_eq[U8](3, a)
      h.assert_eq[USize](3, b.size())
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestThenConst is UnitTest
  fun name(): String => "binary_codec/ThenConst"

  fun apply(h: TestHelper) =>
    // 3 bytes of data followed by literal "UD"
    let data: Array[U8] val = [0x01; 0x02; 0x03; 0x55; 0x44]
    let codec =
      ThenConst[Array[U8] val](
        Bytes(3),
        Literal([as U8: 'U'; 'D']))
    match codec.decode(data, 0)
    | (let v: Array[U8] val, let c: USize) =>
      h.assert_eq[USize](3, v.size())
      h.assert_eq[USize](40, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestConstThen is UnitTest
  fun name(): String => "binary_codec/ConstThen"

  fun apply(h: TestHelper) =>
    // Literal 0x47 (MPEG sync) followed by U8
    let data: Array[U8] val = [0x47; 0xFF]
    let codec = ConstThen[U8](Literal([as U8: 0x47]), U8Codec)
    match codec.decode(data, 0)
    | (let v: U8, let c: USize) =>
      h.assert_eq[U8](0xFF, v)
      h.assert_eq[USize](16, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestMany is UnitTest
  fun name(): String => "binary_codec/Many"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x01; 0x02; 0x03]
    let codec = Many[U8](U8Codec)
    match codec.decode(data, 0)
    | (let v: Array[U8] val, let c: USize) =>
      h.assert_eq[USize](3, v.size())
      try
        h.assert_eq[U8](1, v(0)?)
        h.assert_eq[U8](2, v(1)?)
        h.assert_eq[U8](3, v(2)?)
      else
        h.fail("element access failed")
      end
      h.assert_eq[USize](24, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestCount is UnitTest
  fun name(): String => "binary_codec/Count"

  fun apply(h: TestHelper) =>
    // U16BE count (2), then 2 U8 values
    let data: Array[U8] val = [0x00; 0x02; 0xAA; 0xBB]
    let codec = Count[U8, U16](U16BE, U8Codec)
    match codec.decode(data, 0)
    | (let v: Array[U8] val, let c: USize) =>
      h.assert_eq[USize](2, v.size())
      try
        h.assert_eq[U8](0xAA, v(0)?)
        h.assert_eq[U8](0xBB, v(1)?)
      else
        h.fail("element access failed")
      end
      h.assert_eq[USize](32, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestCountRoundtrip is UnitTest
  fun name(): String => "binary_codec/Count/roundtrip"

  fun apply(h: TestHelper) =>
    let codec = Count[U8, U16](U16BE, U8Codec)
    let value: Array[U8] val = [0xAA; 0xBB; 0xCC]
    match \exhaustive\ codec.encode(value)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](5, encoded.size())
      match \exhaustive\ codec.decode(encoded, 0)
      | (let v: Array[U8] val, _) =>
        h.assert_eq[USize](3, v.size())
        try
          h.assert_eq[U8](0xAA, v(0)?)
          h.assert_eq[U8](0xBB, v(1)?)
          h.assert_eq[U8](0xCC, v(2)?)
        else
          h.fail("element access failed")
        end
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestCountOverflow is UnitTest
  fun name(): String => "binary_codec/Count/overflow"

  fun apply(h: TestHelper) =>
    let codec = Count[U8, U8](U8Codec, U8Codec)
    let big = recover val Array[U8].init(0, 256) end
    match \exhaustive\ codec.encode(big)
    | let _: Array[U8] val => h.fail("expected EncodeError for 256 elements")
    | let _: EncodeError => None
    end

    let at_max = recover val Array[U8].init(0, 255) end
    match \exhaustive\ codec.encode(at_max)
    | let encoded: Array[U8] val =>
      match \exhaustive\ codec.decode(encoded, 0)
      | (let v: Array[U8] val, _) =>
        h.assert_eq[USize](255, v.size())
      else h.fail("decode failed")
      end
    | let e: EncodeError =>
      h.fail("encode at max failed: " + e.string())
    end

class \nodoc\ iso _TestCountU32 is UnitTest
  fun name(): String => "binary_codec/Count/U32"

  fun apply(h: TestHelper) =>
    let codec = Count[U8, U32](U32BE, U8Codec)
    let value: Array[U8] val = [0x01; 0x02]
    match \exhaustive\ codec.encode(value)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](6, encoded.size())
      match \exhaustive\ codec.decode(encoded, 0)
      | (let v: Array[U8] val, _) =>
        h.assert_eq[USize](2, v.size())
        try
          h.assert_eq[U8](0x01, v(0)?)
          h.assert_eq[U8](0x02, v(1)?)
        else
          h.fail("element access failed")
        end
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestCountDecodeOverflow is UnitTest
  fun name(): String => "binary_codec/Count/decode_overflow"

  fun apply(h: TestHelper) =>
    let codec = Count[U8, U128](U128BE, U8Codec)
    // U128 count = 0x0000...0001_00000000_00000000 (exceeds USize on 64-bit)
    let data: Array[U8] val =
      [
        0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x01
        0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00
      ]
    match \exhaustive\ codec.decode(data, 0)
    | (let _: Array[U8] val, _) =>
      h.fail("expected MalformedData for count exceeding USize")
    | let _: MalformedData => None
    | InsufficientData => h.fail("expected MalformedData, got InsufficientData")
    end

class \nodoc\ iso _TestConditional is UnitTest
  fun name(): String => "binary_codec/Conditional"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0x42]

    let present = Conditional[U8](true, U8Codec)
    match present.decode(data, 0)
    | (let v: (U8 | None), let c: USize) =>
      match v
      | let u: U8 => h.assert_eq[U8](0x42, u)
      else h.fail("expected U8")
      end
      h.assert_eq[USize](8, c)
    else
      h.fail("decode present failed")
    end

    let absent = Conditional[U8](false, U8Codec)
    match absent.decode(data, 0)
    | (let v: (U8 | None), let c: USize) =>
      match v
      | None => None
      else h.fail("expected None")
      end
      h.assert_eq[USize](0, c)
    else
      h.fail("decode absent failed")
    end

class \nodoc\ iso _TestSkipBits is UnitTest
  fun name(): String => "binary_codec/SkipBits"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [0xFF]
    let codec = SkipBits(4)
    match codec.decode(data, 0)
    | (None, let c: USize) =>
      h.assert_eq[USize](4, c)
    else
      h.fail("skip failed")
    end

class \nodoc\ iso _TestVariableSize is UnitTest
  fun name(): String => "binary_codec/VariableSize"

  fun apply(h: TestHelper) =>
    // U16BE length (5), then 5 bytes
    let data: Array[U8] val =
      [0x00; 0x05; 0x48; 0x65; 0x6C; 0x6C; 0x6F]
    let codec = VariableSize[Array[U8] val, U16](U16BE, BytesRest)
    match codec.decode(data, 0)
    | (let v: Array[U8] val, let c: USize) =>
      h.assert_eq[USize](5, v.size())
      h.assert_eq[USize](56, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestVariableSizeRoundtrip is UnitTest
  fun name(): String => "binary_codec/VariableSize/roundtrip"

  fun apply(h: TestHelper) =>
    let codec = VariableSize[Array[U8] val, U16](U16BE, BytesRest)
    let value: Array[U8] val = [0x48; 0x65; 0x6C; 0x6C; 0x6F]
    match \exhaustive\ codec.encode(value)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](7, encoded.size())
      match \exhaustive\ codec.decode(encoded, 0)
      | (let v: Array[U8] val, _) =>
        h.assert_eq[USize](5, v.size())
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestVariableSizeOverflow is UnitTest
  fun name(): String => "binary_codec/VariableSize/overflow"

  fun apply(h: TestHelper) =>
    let codec = VariableSize[Array[U8] val, U8](U8Codec, BytesRest)
    let big = recover val Array[U8].init(0, 256) end
    match \exhaustive\ codec.encode(big)
    | let _: Array[U8] val =>
      h.fail("expected EncodeError for 256-byte body")
    | let _: EncodeError => None
    end

    let at_max = recover val Array[U8].init(0, 255) end
    match \exhaustive\ codec.encode(at_max)
    | let encoded: Array[U8] val =>
      match \exhaustive\ codec.decode(encoded, 0)
      | (let v: Array[U8] val, _) =>
        h.assert_eq[USize](255, v.size())
      else h.fail("decode failed")
      end
    | let e: EncodeError =>
      h.fail("encode at max failed: " + e.string())
    end

class \nodoc\ iso _TestVariableSizeU32 is UnitTest
  fun name(): String => "binary_codec/VariableSize/U32"

  fun apply(h: TestHelper) =>
    let codec = VariableSize[Array[U8] val, U32](U32BE, BytesRest)
    let value: Array[U8] val = [0x01; 0x02; 0x03]
    match \exhaustive\ codec.encode(value)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](7, encoded.size())
      match \exhaustive\ codec.decode(encoded, 0)
      | (let v: Array[U8] val, _) =>
        h.assert_eq[USize](3, v.size())
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestVariableSizeDecodeOverflow is UnitTest
  fun name(): String => "binary_codec/VariableSize/decode_overflow"

  fun apply(h: TestHelper) =>
    let codec = VariableSize[Array[U8] val, U128](U128BE, BytesRest)
    // U128 size = 0x0000...0001_00000000_00000000 (exceeds USize on 64-bit)
    let data: Array[U8] val =
      [
        0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x01
        0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00; 0x00
      ]
    match \exhaustive\ codec.decode(data, 0)
    | (let _: Array[U8] val, _) =>
      h.fail("expected MalformedData for size exceeding USize")
    | let _: MalformedData => None
    | InsufficientData => h.fail("expected MalformedData, got InsufficientData")
    end

class \nodoc\ iso _TestFrameDecoder is UnitTest
  fun name(): String => "binary_codec/FrameDecoder"

  fun apply(h: TestHelper) =>
    let codec = Seq2[U8, U8](U8Codec, U8Codec)
    let framer = FrameDecoder[(U8, U8)](codec)
    let notify: _TestFrameCollector ref = _TestFrameCollector

    // Send 3 bytes: first 2 make a complete frame, third is partial
    let chunk1: Array[U8] iso = recover [as U8: 0x01; 0x02; 0x03] end
    framer.received(consume chunk1, notify)
    h.assert_eq[USize](1, notify.frames.size())
    try
      h.assert_eq[U8](0x01, notify.frames(0)?._1)
      h.assert_eq[U8](0x02, notify.frames(0)?._2)
    else
      h.fail("frame access failed")
    end

    // Send 1 more byte to complete second frame
    let chunk2: Array[U8] iso = recover [as U8: 0x04] end
    framer.received(consume chunk2, notify)
    h.assert_eq[USize](2, notify.frames.size())
    try
      h.assert_eq[U8](0x03, notify.frames(1)?._1)
      h.assert_eq[U8](0x04, notify.frames(1)?._2)
    else
      h.fail("second frame access failed")
    end

class \nodoc\ ref _TestFrameCollector is FrameNotify[(U8, U8)]
  embed frames: Array[(U8, U8)] = Array[(U8, U8)]
  var errors: USize = 0

  fun ref frame_received(value: (U8, U8)) =>
    frames.push(value)

  fun ref frame_error(err: MalformedData) =>
    errors = errors + 1

class \nodoc\ ref _TestU8FrameCollector is FrameNotify[U8]
  embed frames: Array[U8] = Array[U8]
  var errors: USize = 0

  fun ref frame_received(value: U8) =>
    frames.push(value)

  fun ref frame_error(err: MalformedData) =>
    errors = errors + 1

class \nodoc\ iso _TestFrameDecoderSubByte is UnitTest
  """
  4-bit frames: two nibbles decoded from a single byte.
  """
  fun name(): String => "binary_codec/FrameDecoder/SubByte"

  fun apply(h: TestHelper) =>
    let framer = FrameDecoder[U8](BitsU8(4))
    let notify: _TestU8FrameCollector ref = _TestU8FrameCollector

    // 0xAB = 1010 1011 → two 4-bit frames: 0xA (10) then 0xB (11)
    let chunk: Array[U8] iso = recover [as U8: 0xAB] end
    framer.received(consume chunk, notify)
    h.assert_eq[USize](0, notify.errors)
    h.assert_eq[USize](2, notify.frames.size())
    try
      h.assert_eq[U8](0x0A, notify.frames(0)?)
      h.assert_eq[U8](0x0B, notify.frames(1)?)
    else
      h.fail("frame access failed")
    end

class \nodoc\ iso _TestFrameDecoderNonAligned is UnitTest
  """
  12-bit frames from 3 bytes. Seq2(BitsU8(4), BitsU8(8)) is 12 bits
  per frame. Input AB CD EF → frame 1 at bits 0..11 = (0xA, 0xBC),
  frame 2 at bits 12..23 = (0xD, 0xEF).
  """
  fun name(): String => "binary_codec/FrameDecoder/NonAligned"

  fun apply(h: TestHelper) =>
    let codec = Seq2[U8, U8](BitsU8(4), BitsU8(8))
    let framer = FrameDecoder[(U8, U8)](codec)
    let notify: _TestFrameCollector ref = _TestFrameCollector

    let chunk: Array[U8] iso = recover [as U8: 0xAB; 0xCD; 0xEF] end
    framer.received(consume chunk, notify)
    h.assert_eq[USize](0, notify.errors)
    h.assert_eq[USize](2, notify.frames.size())
    try
      h.assert_eq[U8](0x0A, notify.frames(0)?._1)
      h.assert_eq[U8](0xBC, notify.frames(0)?._2)
      h.assert_eq[U8](0x0D, notify.frames(1)?._1)
      h.assert_eq[U8](0xEF, notify.frames(1)?._2)
    else
      h.fail("frame access failed")
    end

class \nodoc\ iso _TestFrameDecoderSubByteRemainder is UnitTest
  """
  Sub-byte remainder carried across `received()` calls. Two bytes
  hold one 12-bit frame with 4 remainder bits; the next byte
  completes a second frame from those 4 bits plus 8 new ones.
  """
  fun name(): String => "binary_codec/FrameDecoder/SubByteRemainder"

  fun apply(h: TestHelper) =>
    let codec = Seq2[U8, U8](BitsU8(4), BitsU8(8))
    let framer = FrameDecoder[(U8, U8)](codec)
    let notify: _TestFrameCollector ref = _TestFrameCollector

    // First call: 2 bytes = 16 bits. One 12-bit frame, 4 bits left over.
    // 0xAB 0xCD → bits: 1010 1011 1100 1101
    // Frame 1 (bits 0..11): 4-bit = 1010 (0xA), 8-bit = 1011 1100 (0xBC)
    // Remainder: 4 bits = 1101
    let chunk1: Array[U8] iso = recover [as U8: 0xAB; 0xCD] end
    framer.received(consume chunk1, notify)
    h.assert_eq[USize](0, notify.errors)
    h.assert_eq[USize](1, notify.frames.size())
    try
      h.assert_eq[U8](0x0A, notify.frames(0)?._1)
      h.assert_eq[U8](0xBC, notify.frames(0)?._2)
    else
      h.fail("first chunk frame access failed")
    end

    // Second call: 1 byte = 8 bits. Combined with 4 remainder bits = 12.
    // Remainder bits: 1101, new byte 0xEF = 1110 1111
    // Frame 2 (bits 0..11): 4-bit = 1101 (0xD), 8-bit = 1110 1111 (0xEF)
    let chunk2: Array[U8] iso = recover [as U8: 0xEF] end
    framer.received(consume chunk2, notify)
    h.assert_eq[USize](0, notify.errors)
    h.assert_eq[USize](2, notify.frames.size())
    try
      h.assert_eq[U8](0x0D, notify.frames(1)?._1)
      h.assert_eq[U8](0xEF, notify.frames(1)?._2)
    else
      h.fail("second chunk frame access failed")
    end

class \nodoc\ iso _TestFrameDecoderOddBits is UnitTest
  """
  3-bit frames from a single byte. The frame size does not evenly
  divide 8, so the sub-byte offset cycles through values other than
  0 and 4.
  """
  fun name(): String => "binary_codec/FrameDecoder/OddBits"

  fun apply(h: TestHelper) =>
    let framer = FrameDecoder[U8](BitsU8(3))
    let notify: _TestU8FrameCollector ref = _TestU8FrameCollector

    // 0xE3 = 111_000_11 → two 3-bit frames: 7 then 0, 2 remainder bits
    let chunk: Array[U8] iso = recover [as U8: 0xE3] end
    framer.received(consume chunk, notify)
    h.assert_eq[USize](0, notify.errors)
    h.assert_eq[USize](2, notify.frames.size())
    try
      h.assert_eq[U8](7, notify.frames(0)?)
      h.assert_eq[U8](0, notify.frames(1)?)
    else
      h.fail("frame access failed")
    end

class \nodoc\ iso _TestFrameDecoderMalformedRecovery is UnitTest
  """
  After MalformedData the decoder discards its buffer, so the next
  received() starts fresh and decodes valid data normally.
  """
  fun name(): String => "binary_codec/FrameDecoder/MalformedRecovery"

  fun apply(h: TestHelper) =>
    let codec =
      ConstThen[U8](
        Literal(recover val [as U8: 0xFF] end), U8Codec)
    let framer = FrameDecoder[U8](codec)
    let notify: _TestU8FrameCollector ref = _TestU8FrameCollector

    // Bad sync byte triggers MalformedData
    framer.received(recover [as U8: 0x00; 0x42] end, notify)
    h.assert_eq[USize](1, notify.errors)
    h.assert_eq[USize](0, notify.frames.size())

    // Valid frame decodes from the fresh buffer
    framer.received(recover [as U8: 0xFF; 0x42] end, notify)
    h.assert_eq[USize](1, notify.errors)
    h.assert_eq[USize](1, notify.frames.size())
    try
      h.assert_eq[U8](0x42, notify.frames(0)?)
    else
      h.fail("frame access failed")
    end

class \nodoc\ iso _TestFrameDecoderMalformedAfterGood is UnitTest
  """
  Good frames decoded before MalformedData are delivered normally.
  The decoder discards its buffer after the error, so the next
  received() recovers.
  """
  fun name(): String => "binary_codec/FrameDecoder/MalformedAfterGood"

  fun apply(h: TestHelper) =>
    let codec =
      ConstThen[U8](
        Literal(recover val [as U8: 0xFF] end), U8Codec)
    let framer = FrameDecoder[U8](codec)
    let notify: _TestU8FrameCollector ref = _TestU8FrameCollector

    // One valid frame then bad sync in the same chunk
    framer.received(
      recover [as U8: 0xFF; 0x42; 0x00; 0xBB] end, notify)
    h.assert_eq[USize](1, notify.frames.size())
    h.assert_eq[USize](1, notify.errors)
    try
      h.assert_eq[U8](0x42, notify.frames(0)?)
    else
      h.fail("frame access failed")
    end

    // Recovery: valid frame decodes from fresh buffer
    framer.received(recover [as U8: 0xFF; 0x99] end, notify)
    h.assert_eq[USize](2, notify.frames.size())
    h.assert_eq[USize](1, notify.errors)
    try
      h.assert_eq[U8](0x99, notify.frames(1)?)
    else
      h.fail("recovery frame access failed")
    end

class \nodoc\ iso _TestFrameDecoderMalformedConsecutive is UnitTest
  """
  Two consecutive chunks of bad data produce two distinct errors,
  and the decoder still recovers when valid data arrives.
  """
  fun name(): String => "binary_codec/FrameDecoder/MalformedConsecutive"

  fun apply(h: TestHelper) =>
    let codec =
      ConstThen[U8](
        Literal(recover val [as U8: 0xFF] end), U8Codec)
    let framer = FrameDecoder[U8](codec)
    let notify: _TestU8FrameCollector ref = _TestU8FrameCollector

    // First bad data
    framer.received(recover [as U8: 0x00; 0x42] end, notify)
    h.assert_eq[USize](1, notify.errors)

    // Second bad data — a new error, not the first repeated
    framer.received(recover [as U8: 0x01; 0x99] end, notify)
    h.assert_eq[USize](2, notify.errors)

    // Recovery: valid frame still works
    framer.received(recover [as U8: 0xFF; 0xAA] end, notify)
    h.assert_eq[USize](2, notify.errors)
    h.assert_eq[USize](1, notify.frames.size())
    try
      h.assert_eq[U8](0xAA, notify.frames(0)?)
    else
      h.fail("recovery frame access failed")
    end

class \nodoc\ iso _TestSubByteBitFields is UnitTest
  fun name(): String => "binary_codec/SubByteBitFields"

  fun apply(h: TestHelper) =>
    // First byte of IPv4: version=4 (4 bits), IHL=5 (4 bits)
    // 0x45 = 0100 0101
    let data: Array[U8] val = [0x45]
    let codec = Seq2[U8, U8](BitsU8(4), BitsU8(4))
    match codec.decode(data, 0)
    | ((let a: U8, let b: U8), let c: USize) =>
      h.assert_eq[U8](4, a)
      h.assert_eq[U8](5, b)
      h.assert_eq[USize](8, c)
    else
      h.fail("decode failed")
    end

class \nodoc\ iso _TestRoundtripU8 is UnitTest
  fun name(): String => "binary_codec/roundtrip/U8"

  fun apply(h: TestHelper) =>
    match \exhaustive\ U8Codec.encode(0x42)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](1, encoded.size())
      match U8Codec.decode(encoded, 0)
      | (let v: U8, _) => h.assert_eq[U8](0x42, v)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestRoundtripU16BE is UnitTest
  fun name(): String => "binary_codec/roundtrip/U16BE"

  fun apply(h: TestHelper) =>
    match \exhaustive\ U16BE.encode(0x1234)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](2, encoded.size())
      match U16BE.decode(encoded, 0)
      | (let v: U16, _) => h.assert_eq[U16](0x1234, v)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestRoundtripSeq2 is UnitTest
  fun name(): String => "binary_codec/roundtrip/Seq2"

  fun apply(h: TestHelper) =>
    let codec = Seq2[U8, U16](U8Codec, U16BE)
    let value: (U8, U16) = (0x42, 0x1234)
    match \exhaustive\ codec.encode(value)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](3, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U16), _) =>
        h.assert_eq[U8](0x42, a)
        h.assert_eq[U16](0x1234, b)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestRoundtripBits is UnitTest
  fun name(): String => "binary_codec/roundtrip/Bits"

  fun apply(h: TestHelper) =>
    // Two 4-bit fields make one byte
    let codec = Seq2[U8, U8](BitsU8(4), BitsU8(4))
    let value: (U8, U8) = (4, 5)
    match \exhaustive\ codec.encode(value)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](1, encoded.size())
      try h.assert_eq[U8](0x45, encoded(0)?) else h.fail("byte check") end
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U8), _) =>
        h.assert_eq[U8](4, a)
        h.assert_eq[U8](5, b)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedU8Codec is UnitTest
  fun name(): String => "binary_codec/unaligned/U8Codec"

  fun apply(h: TestHelper) =>
    let codec = Seq3[U8, U8, U8](BitsU8(4), U8Codec, BitsU8(4))
    match \exhaustive\ codec.encode((1, 0x77, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](2, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U8, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U8](0x77, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedI8Codec is UnitTest
  fun name(): String => "binary_codec/unaligned/I8Codec"

  fun apply(h: TestHelper) =>
    let codec = Seq3[U8, I8, U8](BitsU8(4), I8Codec, BitsU8(4))
    match \exhaustive\ codec.encode((1, -5, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](2, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: I8, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[I8](-5, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedU16LE is UnitTest
  fun name(): String => "binary_codec/unaligned/U16LE"

  fun apply(h: TestHelper) =>
    let codec = Seq3[U8, U16, U8](BitsU8(4), U16LE, BitsU8(4))
    match \exhaustive\ codec.encode((1, 0x1234, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](3, encoded.size())
      try
        h.assert_eq[U8](0x13, encoded(0)?)
        h.assert_eq[U8](0x41, encoded(1)?)
        h.assert_eq[U8](0x22, encoded(2)?)
      else
        h.fail("byte access failed")
      end
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U16, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U16](0x1234, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedU32LE is UnitTest
  fun name(): String => "binary_codec/unaligned/U32LE"

  fun apply(h: TestHelper) =>
    let codec = Seq3[U8, U32, U8](BitsU8(4), U32LE, BitsU8(4))
    match \exhaustive\ codec.encode((1, 0x12345678, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](5, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U32, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U32](0x12345678, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedU64LE is UnitTest
  fun name(): String => "binary_codec/unaligned/U64LE"

  fun apply(h: TestHelper) =>
    let codec = Seq3[U8, U64, U8](BitsU8(4), U64LE, BitsU8(4))
    match \exhaustive\ codec.encode((1, 0x123456789ABCDEF0, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](9, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U64, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U64](0x123456789ABCDEF0, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedU128BE is UnitTest
  fun name(): String => "binary_codec/unaligned/U128BE"

  fun apply(h: TestHelper) =>
    let val128: U128 =
      (U128(0x0102030405060708) << 64) or 0x090A0B0C0D0E0F10
    let codec = Seq3[U8, U128, U8](BitsU8(4), U128BE, BitsU8(4))
    match \exhaustive\ codec.encode((1, val128, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](17, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U128, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U128](val128, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedU128LE is UnitTest
  fun name(): String => "binary_codec/unaligned/U128LE"

  fun apply(h: TestHelper) =>
    let val128: U128 =
      (U128(0x0102030405060708) << 64) or 0x090A0B0C0D0E0F10
    let codec = Seq3[U8, U128, U8](BitsU8(4), U128LE, BitsU8(4))
    match \exhaustive\ codec.encode((1, val128, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](17, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U128, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U128](val128, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedBytes is UnitTest
  fun name(): String => "binary_codec/unaligned/Bytes"

  fun apply(h: TestHelper) =>
    let codec =
      Seq3[U8, Array[U8] val, U8](BitsU8(4), Bytes(2), BitsU8(4))
    match \exhaustive\ codec.encode((1, [as U8: 0xAA; 0xBB], 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](3, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: Array[U8] val, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[USize](2, b.size())
        try
          h.assert_eq[U8](0xAA, b(0)?)
          h.assert_eq[U8](0xBB, b(1)?)
        else
          h.fail("byte content access failed")
        end
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedBytesRest is UnitTest
  fun name(): String => "binary_codec/unaligned/BytesRest"

  fun apply(h: TestHelper) =>
    let codec = Seq2[U8, Array[U8] val](BitsU8(4), BytesRest)
    let data: Array[U8] val = [0x1A; 0xAB; 0xB0]
    match codec.decode(data, 0)
    | ((let a: U8, let b: Array[U8] val), let c: USize) =>
      h.assert_eq[U8](1, a)
      h.assert_eq[USize](2, b.size())
      try
        h.assert_eq[U8](0xAA, b(0)?)
        h.assert_eq[U8](0xBB, b(1)?)
      else
        h.fail("byte content access failed")
      end
      h.assert_eq[USize](20, c)
    else h.fail("decode failed")
    end

class \nodoc\ iso _TestUnalignedLiteral is UnitTest
  fun name(): String => "binary_codec/unaligned/Literal"

  fun apply(h: TestHelper) =>
    let codec =
      Seq3[U8, None, U8](BitsU8(4), Literal([as U8: 0xAA]), BitsU8(4))
    match \exhaustive\ codec.encode((1, None, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](2, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, _, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

class \nodoc\ iso _TestUnalignedVariableSize is UnitTest
  fun name(): String => "binary_codec/unaligned/VariableSize"

  fun apply(h: TestHelper) =>
    let codec =
      Seq3[U8, U8, U8](
        BitsU8(4),
        VariableSize[U8, U16](BitsU16(8), U8Codec),
        BitsU8(4))
    match \exhaustive\ codec.encode((1, 0x77, 2))
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](3, encoded.size())
      match codec.decode(encoded, 0)
      | ((let a: U8, let b: U8, let c: U8), _) =>
        h.assert_eq[U8](1, a)
        h.assert_eq[U8](0x77, b)
        h.assert_eq[U8](2, c)
      else h.fail("decode failed")
      end
    | let e: EncodeError => h.fail("encode failed: " + e.string())
    end

type _TestMessage is (_TestMsgA | _TestMsgB)

class val _TestMsgA
  let value: U16

  new val create(v: U16) => value = v

class val _TestMsgB
  let flag: U8

  new val create(f: U8) => flag = f

class \nodoc\ iso _TestDiscriminated is UnitTest
  fun name(): String => "binary_codec/Discriminated"

  fun apply(h: TestHelper) =>
    let builder = DiscriminatedBuilder[U8, _TestMessage](U8Codec)
    builder
      .case_of[_TestMsgA](
        0x01,
        Mapped[U16, _TestMsgA](
          U16BE,
          {(v: U16): _TestMsgA => _TestMsgA(v) },
          {(m: _TestMsgA): U16 => m.value }),
        {(m: _TestMessage): _TestMsgA ? => m as _TestMsgA })
      .case_of[_TestMsgB](
        0x02,
        Mapped[U8, _TestMsgB](
          U8Codec,
          {(v: U8): _TestMsgB => _TestMsgB(v) },
          {(m: _TestMsgB): U8 => m.flag }),
        {(m: _TestMessage): _TestMsgB ? => m as _TestMsgB })

    let codec = builder.build()

    let data_a: Array[U8] val = [0x01; 0x01; 0x00]
    match codec.decode(data_a, 0)
    | (let v: _TestMsgA, let c: USize) =>
      h.assert_eq[U16](256, v.value)
      h.assert_eq[USize](24, c)
    | (let v: _TestMsgB, _) =>
      h.fail("expected MsgA, got MsgB")
    else
      h.fail("decode MsgA failed")
    end

    let data_b: Array[U8] val = [0x02; 0xFF]
    match codec.decode(data_b, 0)
    | (let v: _TestMsgB, let c: USize) =>
      h.assert_eq[U8](0xFF, v.flag)
      h.assert_eq[USize](16, c)
    | (let v: _TestMsgA, _) =>
      h.fail("expected MsgB, got MsgA")
    else
      h.fail("decode MsgB failed")
    end

    let data_bad: Array[U8] val = [0x03; 0xFF]
    match codec.decode(data_bad, 0)
    | let m: MalformedData => None
    else
      h.fail("expected MalformedData for unknown tag")
    end

    let msg_a: _TestMessage = _TestMsgA(0x1234)
    match \exhaustive\ codec.encode(msg_a)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](3, encoded.size())
      match codec.decode(encoded, 0)
      | (let v: _TestMsgA, _) =>
        h.assert_eq[U16](0x1234, v.value)
      else h.fail("roundtrip decode MsgA failed")
      end
    | let e: EncodeError =>
      h.fail("encode MsgA failed: " + e.string())
    end

    let msg_b: _TestMessage = _TestMsgB(0xAB)
    match \exhaustive\ codec.encode(msg_b)
    | let encoded: Array[U8] val =>
      h.assert_eq[USize](2, encoded.size())
      match codec.decode(encoded, 0)
      | (let v: _TestMsgB, _) =>
        h.assert_eq[U8](0xAB, v.flag)
      else h.fail("roundtrip decode MsgB failed")
      end
    | let e: EncodeError =>
      h.fail("encode MsgB failed: " + e.string())
    end
