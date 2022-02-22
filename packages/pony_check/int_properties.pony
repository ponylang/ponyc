primitive _StringifyIntArg
  fun apply(choice: U8, int: U128): String iso ^ =>
   let num =
     match choice % 14
     | 0 => "U8(" + int.u8().string() + ")"
     | 1 => "U16(" + int.u16().string() + ")"
     | 2 => "U32(" + int.u32().string() + ")"
     | 3 => "U64(" + int.u64().string() + ")"
     | 4 => "ULong(" + int.ulong().string() + ")"
     | 5 => "USize(" + int.usize().string() + ")"
     | 6 => "U128(" + int.string() + ")"
     | 7 => "I8(" + int.i8().string() + ")"
     | 8 => "I16(" + int.i16().string() + ")"
     | 9 => "I32(" + int.i32().string() + ")"
     | 10 => "I64(" + int.i64().string() + ")"
     | 11 => "ILong(" + int.ilong().string() + ")"
     | 12 => "ISize(" + int.isize().string() + ")"
     | 13 => "I128(" + int.i128().string() + ")"
     else
       ""
     end
   num.clone()

class IntPropertySample is Stringable
  let choice: U8
  let int: U128

  new create(choice': U8, int': U128) =>
    choice = choice'
    int = int'

  fun string(): String iso^ =>
    _StringifyIntArg(choice, int)

type IntUnitTest is Property1UnitTest[IntPropertySample]

trait IntProperty is Property1[IntPropertySample]
  """
  A property implementation for conveniently evaluating properties
  for all Pony Integer types at once.

  The property needs to be formulated inside the method `int_property`:

  ```pony
  class DivisionByZeroProperty is IntProperty
    fun name(): String => "div/0"

    fun int_property[T: (Int & Integer[T] val)](x: T, h: PropertyHelper)? =>
      h.assert_eq[T](T.from[U8](0), x / T.from[U8](0))
  ```
  """
  fun gen(): Generator[IntPropertySample] =>
    Generators.map2[U8, U128, IntPropertySample](
      Generators.u8(),
      Generators.u128(),
      {(choice, int) => IntPropertySample(choice, int) })

  fun ref property(sample: IntPropertySample, h: PropertyHelper) ? =>
    let x = sample.int
    match sample.choice % 14
    | 0 => int_property[U8](x.u8(), h)?
    | 1 => int_property[U16](x.u16(), h)?
    | 2 => int_property[U32](x.u32(), h)?
    | 3 => int_property[U64](x.u64(), h)?
    | 4 => int_property[ULong](x.ulong(), h)?
    | 5 => int_property[USize](x.usize(), h)?
    | 6 => int_property[U128](x, h)?
    | 7 => int_property[I8](x.i8(), h)?
    | 8 => int_property[I16](x.i16(), h)?
    | 9 => int_property[I32](x.i32(), h)?
    | 10 => int_property[I64](x.i64(), h)?
    | 11 => int_property[ILong](x.ilong(), h)?
    | 12 => int_property[ISize](x.isize(), h)?
    | 13 => int_property[I128](x.i128(), h)?
    else
      h.log("rem is broken")
      error
    end

  fun ref int_property[T: (Int & Integer[T] val)](x: T, h: PropertyHelper)?

class IntPairPropertySample is Stringable
  let choice: U8
  let int1: U128
  let int2: U128

  new create(choice': U8, int1': U128, int2': U128) =>
    choice = choice'
    int1 = int1'
    int2 = int2'

  fun string(): String iso^ =>
    let num1: String val = _StringifyIntArg(choice, int1)
    let num2: String val = _StringifyIntArg(choice, int2)
    "".join(["("; num1; ", "; num2; ")"].values())


type IntPairUnitTest is Property1UnitTest[IntPairPropertySample]

trait IntPairProperty is Property1[IntPairPropertySample]
  """
  A property implementation for conveniently evaluating properties
  for pairs of integers of all Pony integer types at once.

  The property needs to be formulated inside the method `int_property`:

  ```pony
  class CommutativeMultiplicationProperty is IntPairProperty
    fun name(): String => "commutativity/mul"

    fun int_property[T: (Int & Integer[T] val)](x: T, y: T, h: PropertyHelper)? =>
      h.assert_eq[T](x * y, y * x)
  ```
  """
  fun gen(): Generator[IntPairPropertySample] =>
    Generators.map3[U8, U128, U128, IntPairPropertySample](
      Generators.u8(),
      Generators.u128(),
      Generators.u128(),
      {(choice, int1, int2) => IntPairPropertySample(choice, int1, int2) })

  fun ref property(sample: IntPairPropertySample, h: PropertyHelper) ? =>
    let x = sample.int1
    let y = sample.int2
    match sample.choice % 14
    | 0 => int_property[U8](x.u8(), y.u8(), h)?
    | 1 => int_property[U16](x.u16(), y.u16(), h)?
    | 2 => int_property[U32](x.u32(), y.u32(), h)?
    | 3 => int_property[U64](x.u64(), y.u64(), h)?
    | 4 => int_property[ULong](x.ulong(), y.ulong(), h)?
    | 5 => int_property[USize](x.usize(), y.usize(), h)?
    | 6 => int_property[U128](x, y, h)?
    | 7 => int_property[I8](x.i8(), y.i8(), h)?
    | 8 => int_property[I16](x.i16(), y.i16(), h)?
    | 9 => int_property[I32](x.i32(), y.i32(), h)?
    | 10 => int_property[I64](x.i64(), y.i64(), h)?
    | 11 => int_property[ILong](x.ilong(), y.ilong(), h)?
    | 12 => int_property[ISize](x.isize(), y.isize(), h)?
    | 13 => int_property[I128](x.i128(), y.i128(), h)?
    else
      h.log("rem is broken")
      error
    end

  fun ref int_property[T: (Int & Integer[T] val)](x: T, y: T, h: PropertyHelper)?
