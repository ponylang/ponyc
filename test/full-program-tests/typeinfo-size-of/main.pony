"""
TypeInfo.size_of[T]() returns the in-memory size of an instance of T.

Only architecture-independent sizes are asserted directly; sizes that
depend on pointer width are derived from `USize(0).bytewidth()`.
"""
use "pony_test"

class _ClassTwoU64
  let a: U64 = 0
  let b: U64 = 0

struct _StructTwoU64
  var a: U64 = 0
  var b: U64 = 0

struct \packed\ _StructU8U64Packed
  var a: U8 = 0
  var b: U64 = 0

struct _StructU8U64
  var a: U8 = 0
  var b: U64 = 0

struct _StructEmbeds
  embed a: _StructTwoU64 = _StructTwoU64
  embed b: _StructU8U64 = _StructU8U64

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_TestMachineWords)
    test(_TestTuples)
    test(_TestPrimitive)
    test(_TestClass)
    test(_TestStructs)

class \nodoc\ iso _TestMachineWords is UnitTest
  """
  Machine words report their unboxed primitive size, never the boxed
  representation.
  """
  fun name(): String => "TypeInfo/size_of/machine-words"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](1, TypeInfo.size_of[U8]())
    h.assert_eq[USize](2, TypeInfo.size_of[U16]())
    h.assert_eq[USize](4, TypeInfo.size_of[U32]())
    h.assert_eq[USize](8, TypeInfo.size_of[U64]())
    h.assert_eq[USize](16, TypeInfo.size_of[U128]())
    h.assert_eq[USize](1, TypeInfo.size_of[I8]())
    h.assert_eq[USize](8, TypeInfo.size_of[I64]())
    h.assert_eq[USize](4, TypeInfo.size_of[F32]())
    h.assert_eq[USize](8, TypeInfo.size_of[F64]())

class \nodoc\ iso _TestTuples is UnitTest
  """
  Tuples are unboxed: a `(U8, U8, U8)` packs to 3 bytes, two `U64`s to 16.
  """
  fun name(): String => "TypeInfo/size_of/tuples"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](3, TypeInfo.size_of[(U8, U8, U8)]())
    h.assert_eq[USize](16, TypeInfo.size_of[(U64, U64)]())
    h.assert_eq[USize](16, TypeInfo.size_of[(F32, F32, F32, F32)]())

class \nodoc\ iso _TestPrimitive is UnitTest
  """
  A non-machine-word primitive is just a type-descriptor pointer.
  """
  fun name(): String => "TypeInfo/size_of/primitive"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](USize(0).bytewidth(), TypeInfo.size_of[None]())

class \nodoc\ iso _TestClass is UnitTest
  """
  A class is a descriptor pointer plus its fields. With two `U64` fields the
  layout is 24 bytes on every supported architecture: on lp64 a pointer (8)
  plus the two fields (16); on 32-bit a 4-byte pointer plus 4 bytes of
  alignment padding plus 16 bytes of fields = 24.
  """
  fun name(): String => "TypeInfo/size_of/class"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](24, TypeInfo.size_of[_ClassTwoU64]())

class \nodoc\ iso _TestStructs is UnitTest
  """
  Structs use C-compatible layout with no descriptor. Two `U64`s pack to 16.
  A `U8` followed by a `U64` pads to 16 (1 + 7 padding + 8). Embedded
  structs nest in place without any extra header.

  Note that structs cannot be passed through a generic helper because Pony
  forbids them as type arguments to ordinary generic methods; only
  `TypeInfo.size_of` accepts them as a special case, so each call uses a
  literal type.
  """
  fun name(): String => "TypeInfo/size_of/structs"

  fun apply(h: TestHelper) =>
    h.assert_eq[USize](16, TypeInfo.size_of[_StructTwoU64]())
    h.assert_eq[USize](16, TypeInfo.size_of[_StructU8U64]())
    h.assert_eq[USize](9, TypeInfo.size_of[_StructU8U64Packed]())
    h.assert_eq[USize](32, TypeInfo.size_of[_StructEmbeds]())
