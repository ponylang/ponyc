use "ponytest"
use "buffered"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestPackUnpack)
    test(_TestParseFormat)

class iso _TestPackUnpack is UnitTest
  fun name(): String => "struct/pack_unpack"

  fun apply(h: TestHelper) ? =>
   // test a hand made string
    let fmt1 = "<Iq10s5pfd"
    let ar_u8_1: Array[U8] val = recover val [as U8: 9;9;9;9;9] end
    let args1: Array[_Packable val] = recover
                [ U32(10)
                  I64(100)
                  "abcdefghij"
                  ar_u8_1
                  F32(0.5)
                  F64(0.25)] end
    let o1 = Struct.pack(fmt1, args1)?
    let r1 = Reader
    for bs in o1.values() do
      r1.append(consume bs)
    end
    let u1 = Struct.unpack(fmt1, consume r1)?
    h.assert_eq[U32](args1(0)? as U32, u1(0)? as U32)
    h.assert_eq[I64](args1(1)? as I64, u1(1)? as I64)
    h.assert_eq[String](args1(2)? as String, u1(2)? as String)
    let u1_u8: Array[U8] val = u1(3)? as Array[U8] val
    for idx in u1_u8.keys() do
      h.assert_eq[U8](ar_u8_1(idx)?, u1_u8(idx)?)
    end
    h.assert_eq[F32](args1(4)? as F32, u1(4)? as F32)
    h.assert_eq[F64](args1(5)? as F64, u1(5)? as F64)

    // test one sample for each type

    // pad byte: x
    var r = Reader
    var packed = Struct.pack("xs", ["a"])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](2, r.size())
    var unpacked = Struct.unpack(">xs", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[String]("a", unpacked(0)? as String)

    // char (string of size 1): c
    r = Reader
    packed = Struct.pack("c", ["a"])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](1, r.size())
    unpacked = Struct.unpack("c", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[String]("a", unpacked(0)? as String)

    // I8: b
    r = Reader
    packed = Struct.pack("b", [I8(-1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](1, r.size())
    unpacked = Struct.unpack("b", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[I8](-1, unpacked(0)? as I8)

    // U8: B
    r = Reader
    packed = Struct.pack("B", [U8(1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](1, r.size())
    unpacked = Struct.unpack("B", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[U8](1, unpacked(0)? as U8)

    // Bool: ?
    r = Reader
    packed = Struct.pack("??", [true; false])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](2, r.size())
    unpacked = Struct.unpack("??", consume r)?
    h.assert_eq[USize](2, unpacked.size())
    h.assert_eq[Bool](true, unpacked(0)? as Bool)
    h.assert_eq[Bool](false, unpacked(1)? as Bool)

    // I16: h
    r = Reader
    packed = Struct.pack("h", [I16(-1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](2, r.size())
    unpacked = Struct.unpack("h", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[I16](-1, unpacked(0)? as I16)

    // U16: H
    r = Reader
    packed = Struct.pack("H", [U16(1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](2, r.size())
    unpacked = Struct.unpack("H", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[U16](1, unpacked(0)? as U16)

    // I32: i, l
    r = Reader
    packed = Struct.pack("il", [I32(-1); I32(-2)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](8, r.size())
    unpacked = Struct.unpack("il", consume r)?
    h.assert_eq[USize](2, unpacked.size())
    h.assert_eq[I32](-1, unpacked(0)? as I32)
    h.assert_eq[I32](-2, unpacked(1)? as I32)

    // U32: I, L
    r = Reader
    packed = Struct.pack("IL", [U32(1); U32(2)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](8, r.size())
    unpacked = Struct.unpack("IL", consume r)?
    h.assert_eq[USize](2, unpacked.size())
    h.assert_eq[U32](1, unpacked(0)? as U32)
    h.assert_eq[U32](2, unpacked(1)? as U32)


    // I64: q
    r = Reader
    packed = Struct.pack("q", [I64(-1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](8, r.size())
    unpacked = Struct.unpack("q", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[I64](-1, unpacked(0)? as I64)

    // U64: Q
    r = Reader
    packed = Struct.pack("Q", [U64(1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](8, r.size())
    unpacked = Struct.unpack("Q", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[U64](1, unpacked(0)? as U64)

    // I128: u
    r = Reader
    packed = Struct.pack("u", [I128(-1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](16, r.size())
    unpacked = Struct.unpack("u", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[I128](-1, unpacked(0)? as I128)

    // U128: U
    r = Reader
    packed = Struct.pack("U", [U128(1)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](16, r.size())
    unpacked = Struct.unpack("U", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[U128](1, unpacked(0)? as U128)

    // F32: f
    r = Reader
    packed = Struct.pack("f", [F32(-0.5)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](4, r.size())
    unpacked = Struct.unpack("f", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[F32](-0.5, unpacked(0)? as F32)

    // F64: d
    r = Reader
    packed = Struct.pack("d", [F64(-0.5)])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](8, r.size())
    unpacked = Struct.unpack("d", consume r)?
    h.assert_eq[USize](1, unpacked.size())
    h.assert_eq[F64](-0.5, unpacked(0)? as F64)

    // String: [length]s  // if length left out, s is 1 char long
    r = Reader
    packed = Struct.pack("s4s", ["a"; "bcde"])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](5, r.size())
    unpacked = Struct.unpack("s4s", consume r)?
    h.assert_eq[USize](2, unpacked.size())
    h.assert_eq[String]("a", unpacked(0)? as String)
    h.assert_eq[String]("bcde", unpacked(1)? as String)

    // Array[U8]: [length]p  // if length left out, p is 1 U8 long
    r = Reader
    packed = Struct.pack("p4p", [[as U8: 1]; [as U8: 2;3;4;5]])?
    for bs in packed.values() do
      r.append(consume bs)
    end
    h.assert_eq[USize](5, r.size())
    unpacked = Struct.unpack("p4p", consume r)?
    h.assert_eq[USize](2, unpacked.size())
    h.assert_eq[U8](1, (unpacked(0)? as Array[U8] val)(0)?)
    for (i, v) in (unpacked(1)? as Array[U8] val).pairs() do
      h.assert_eq[U8](i.u8() + 2, v)
    end

class iso _TestParseFormat is UnitTest
  fun name(): String => "struct/_parseformat"

  fun apply(h: TestHelper) ? =>
    // test handmade strings
    let v = _ParseFormat(">Iq5s10s")?
    let v' = _ParseFormat("<QcbB5s")?

    // test a string of all types in both endiannesses
    let b = _ParseFormat(">xcbB?hHiIlLqQuUfdsp10s10p")?
    let l = _ParseFormat("<xcbB?hHiIlLqQuUfdsp10s10p")?
    // Check the string and char[] types have appropriate lengths
    h.assert_eq[USize](b._2(b._2.size()-1)?._2, 10)
    h.assert_eq[USize](b._2(b._2.size()-2)?._2, 10)
    h.assert_eq[USize](l._2(l._2.size()-1)?._2, 10)
    h.assert_eq[USize](l._2(l._2.size()-2)?._2, 10)

    // Test repetition and length specifiers
    let a3 = _ParseFormat(">3x3c3b3B3?3h3H3i3I3l3L3q3Q3u3U3f3d3s3p")?
    h.assert_eq[USize](3, a3._3)  // 3 padding bytes!
    h.assert_eq[USize](51, a3._2.size())
    for (c, s) in a3._2.values() do
      if "xsp".contains(c) then
        h.assert_eq[USize](3, s)
      else
        h.assert_eq[USize](1, s)
      end
    end
