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
  """
  A sample holding a type choice and an integer value.
  """
  let choice: U8
  let int: U128

  new create(choice': U8, int': U128) =>
    choice = choice'
    int = int'

  fun string(): String iso^ =>
    _StringifyIntArg(choice, int)

trait IntProperty is Property[IntPropertySample]
  """
  Tests a property across all 14 Pony integer types by dispatching each
  sample to `int_property[T]` with the appropriate type.
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

  fun ref int_property[T: (Int & Integer[T] val)](
    x: T,
    h: PropertyHelper)
    ?
    """
    The property to verify for the given integer sample.
    """
