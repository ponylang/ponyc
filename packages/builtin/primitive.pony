type Any is {} tag

primitive None is Stringable
  fun tag string(): String => "None"

primitive True is Stringable
  fun tag string(): String => "True"

primitive False is Stringable
  fun tag string(): String => "False"

type Bool is (True | False)

trait ArithmeticConvertible
  fun tag i8(): I8 => compiler_intrinsic
  fun tag i16(): I16 => compiler_intrinsic
  fun tag i32(): I32 => compiler_intrinsic
  fun tag i64(): I64 => compiler_intrinsic
  fun tag i128(): I128 => compiler_intrinsic

  fun tag u8(): U8 => compiler_intrinsic
  fun tag u16(): U16 => compiler_intrinsic
  fun tag u32(): U32 => compiler_intrinsic
  fun tag u64(): U64 => compiler_intrinsic
  fun tag u128(): U128 => compiler_intrinsic

  fun tag f32(): F32 => compiler_intrinsic
  fun tag f64(): F64 => compiler_intrinsic

primitive SIntLiteral is Stringable, ArithmeticConvertible
  fun tag string(): String iso^ => recover String.from_i128(this, 10) end

  fun tag f32(): F32 => i128().f32()
  fun tag f64(): F64 => i128().f64()

primitive UIntLiteral is Stringable, ArithmeticConvertible
  fun tag string(): String iso^ => recover String.from_u128(this, 10) end

  fun tag f32(): F32 => u128().f32()
  fun tag f64(): F64 => u128().f64()

primitive FloatLiteral is Stringable, ArithmeticConvertible
  fun tag string(): String iso^ => recover String.from_f64(this) end

  fun tag i128(): I128 => f64().i128()
  fun tag u128(): U128 => f64().u128()

type SInt is
  ( SIntLiteral
  | I8
  | I16
  | I32
  | I64
  | I128
  )

type UInt is
  ( UIntLiteral
  | U8
  | U16
  | U32
  | U64
  | U128
  )

type Integer is
  ( SInt
  | UInt
  )

type Float is
  ( FloatLiteral
  | F32
  | F64
  )

type Arithmetic is (Integer | Float)
