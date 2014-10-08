trait Arithmetic[A: Arithmetic[A]]
  fun tag add(y: A): A
  fun tag sub(y: A): A
  fun tag mul(y: A): A
  fun tag div(y: A): A
  fun tag mod(y: A): A
  fun tag neg(): A

trait Logical[A: Logical[A]]
  fun tag and_(y: A): A
  fun tag or_(y: A): A
  fun tag xor_(y: A): A
  fun tag not_(): A

trait Bits[A: Bits[A]] is Logical[A], Arithmetic[A]
  fun tag shl(y: A): A
  fun tag shr(y: A): A

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

trait Real[A: Real[A]] is Stringable, ArithmeticConvertible, Arithmetic[A]
  fun tag add(y: A): A => compiler_intrinsic
  fun tag sub(y: A): A => compiler_intrinsic
  fun tag mul(y: A): A => compiler_intrinsic
  fun tag div(y: A): A => compiler_intrinsic
  fun tag mod(y: A): A => compiler_intrinsic
  fun tag neg(): A => compiler_intrinsic
  fun tag eq(y: A): Bool => compiler_intrinsic
  fun tag ne(y: A): Bool => compiler_intrinsic
  fun tag lt(y: A): Bool => compiler_intrinsic
  fun tag le(y: A): Bool => compiler_intrinsic
  fun tag ge(y: A): Bool => compiler_intrinsic
  fun tag gt(y: A): Bool => compiler_intrinsic

trait Integer[A: Integer[A]] is Real[A], Logical[A], Bits[A]
  fun tag and_(y: A): A => compiler_intrinsic
  fun tag or_(y: A): A => compiler_intrinsic
  fun tag xor_(y: A): A => compiler_intrinsic
  fun tag not_(): A => compiler_intrinsic
  fun tag shl(y: A): A => compiler_intrinsic
  fun tag shr(y: A): A => compiler_intrinsic

type Number is (Signed | Unsigned | Float)

type Signed is (I8 | I16 | I32 | I64 | I128)

type Unsigned is (U8 | U16 | U32 | U64 | U128)

type Float is (F32 | F64)
