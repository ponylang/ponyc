use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    /*
    TODO:
    arrays of union types
    arrays of tags
    no pointers
    no actors
    */
    test(_TestSimple)
    test(_TestArrays)
    test(_TestFailures)

class _MachineWords
  var bool1: Bool = true
  var bool2: Bool = false
  var i8: I8 = 0x3
  var i16: I16 = 0x7BCD
  var i32: I32 = 0x12345678
  var i64: I64 = 0x7EDCBA9876543210
  var i128: I128 = 0x7EDCBA9876543210123456789ABCDEFE
  var ilong: ILong = 1 << (ILong.bitwidth() - 1)
  var isize: ISize = 1 << (ISize.bitwidth() - 1)
  var f32: F32 = 1.2345e-13
  var f64: F64 = 9.82643431e19

  fun eq(that: _MachineWords box): Bool =>
    (bool1 == that.bool1) and
    (bool2 == that.bool2) and
    (i8 == that.i8) and
    (i16 == that.i16) and
    (i32 == that.i32) and
    (i64 == that.i64) and
    (i128 == that.i128) and
    (ilong == that.ilong) and
    (isize == that.isize) and
    (f32 == that.f32) and
    (f64 == that.f64)

class _StructWords
  var u8: U8 = 0x3
  var u16: U16 = 0xABCD
  var u32: U32 = 0x12345678
  var u64: U64 = 0xFEDCBA9876543210
  var u128: U128 = 0xFEDCBA9876543210123456789ABCDEFE
  var ulong: ULong = 1 << (ULong.bitwidth() - 1)
  var usize: USize = 1 << (USize.bitwidth() - 1)

  fun eq(that: _StructWords box): Bool =>
    (u8 == that.u8) and
    (u16 == that.u16) and
    (u32 == that.u32) and
    (u64 == that.u64) and
    (u128 == that.u128) and
    (ulong == that.ulong) and
    (usize == that.usize)

class _Simple
  var words1: _MachineWords = _MachineWords
  embed words2: _MachineWords = _MachineWords
  var words3: _StructWords = _StructWords
  embed words4: _StructWords = _StructWords
  var words5: (Any ref | None) = _MachineWords
  var words6: (Any ref | None) = None
  var string: String = "hello"
  var none: None = None
  var fmt: FormatInt = FormatOctal
  var tuple: (U64, String) = (99, "goodbye")
  var tuple2: ((U64, String) | None) = (101, "awesome")
  var a_tag: _MachineWords tag = words1
  var a_ref: _MachineWords = words1

  fun eq(that: _Simple): Bool =>
    (words1 == that.words1) and
    (words2 == that.words2) and
    (words3 == that.words3) and
    (words4 == that.words4) and
    try
      (words5 as _MachineWords box) == (that.words5 as _MachineWords box)
    else
      false
    end and
    (words6 is that.words6) and
    (string == that.string) and
    (none is that.none) and
    (fmt is that.fmt) and
    (tuple._1 == that.tuple._1) and
    (tuple._2 == that.tuple._2) and
    try
      let x = tuple2 as (U64, String)
      let y = that.tuple2 as (U64, String)
      (x._1 == y._1) and (x._2 == y._2)
    else
      false
    end and
    (a_tag isnt that.a_tag) and
    (a_tag is words1) and
    (that.a_tag is that.words1) and
    (a_ref == that.a_ref) and
    (a_ref is words1) and
    (that.a_ref is that.words1)

class iso _TestSimple is UnitTest
  """
  Test serialising simple fields.
  """
  fun name(): String => "serialise/Simple"

  fun apply(h: TestHelper) ? =>
    let ambient = h.env.root as AmbientAuth
    let serialise = SerialiseAuth(ambient)
    let deserialise = DeserialiseAuth(ambient)

    let x: _Simple = _Simple
    let sx = Serialised(serialise, x)
    let y = sx(deserialise) as _Simple
    h.assert_true(x isnt y)
    h.assert_true(x == y)

class iso _TestArrays is UnitTest
  """
  Test serialising arrays.
  """
  fun name(): String => "serialise/Arrays"

  fun apply(h: TestHelper) ? =>
    let ambient = h.env.root as AmbientAuth
    let serialise = SerialiseAuth(ambient)
    let deserialise = DeserialiseAuth(ambient)

    let x1: Array[U128] = [1, 2, 3]
    var sx = Serialised(serialise, x1)
    let y1 = sx(deserialise) as Array[U128]
    h.assert_true(x1 isnt y1)
    h.assert_array_eq[U128](x1, y1)

    let x2: Array[Bool] = [true, false, true]
    sx = Serialised(serialise, x2)
    let y2 = sx(deserialise) as Array[Bool]
    h.assert_true(x2 isnt y2)
    h.assert_array_eq[Bool](x2, y2)

    let x3: Array[U32] = [1, 2, 3]
    sx = Serialised(serialise, x3)
    let y3 = sx(deserialise) as Array[U32]
    h.assert_true(x3 isnt y3)
    h.assert_array_eq[U32](x3, y3)

    let x4: Array[(U16, Bool)] = [(1, true), (2, false), (3, true)]
    sx = Serialised(serialise, x4)
    let y4 = sx(deserialise) as Array[(U16, Bool)]
    h.assert_true(x4 isnt y4)

    var i = USize(0)
    while i < x4.size() do
      h.assert_eq[U16](x4(i)._1, y4(i)._1)
      h.assert_eq[Bool](x4(i)._2, y4(i)._2)
      i = i + 1
    end

    let x5: Array[String] = ["hi", "there", "folks"]
    sx = Serialised(serialise, x5)
    let y5 = sx(deserialise) as Array[String]
    h.assert_true(x5 isnt y5)
    h.assert_array_eq[String](x5, y5)

    let x6: Array[_StructWords] =
      [as _StructWords: _StructWords, _StructWords, _StructWords]
    sx = Serialised(serialise, x6)
    let y6 = sx(deserialise) as Array[_StructWords]
    h.assert_true(x6 isnt y6)

    i = 0
    while i < x6.size() do
      h.assert_true(x6(i) isnt y6(i))
      h.assert_true(x6(i) == y6(i))
      i = i + 1
    end

actor _EmptyActor

class _HasPointer
  var x: Pointer[U8] = Pointer[U8]

class _HasActor
  var x: _EmptyActor = _EmptyActor

class iso _TestFailures is UnitTest
  """
  Test serialisation failures.
  """
  fun name(): String => "serialise/Failures"

  fun apply(h: TestHelper) ? =>
    let ambient = h.env.root as AmbientAuth
    let serialise = SerialiseAuth(ambient)

    h.assert_error(
      lambda()(serialise)? =>
        Serialised(serialise, _HasPointer)
      end)

    h.assert_error(
      lambda()(serialise)? =>
        Serialised(serialise, _HasActor)
      end)
