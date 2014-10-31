trait Arithmetic[A: Arithmetic[A]]
  fun box add(y: A): A
  fun box sub(y: A): A
  fun box mul(y: A): A
  fun box div(y: A): A
  fun box mod(y: A): A
  fun box neg(): A

trait Logical[A: Logical[A]]
  fun box and_(y: A): A
  fun box or_(y: A): A
  fun box xor_(y: A): A
  fun box not_(): A

trait Bits[A: Bits[A]] is Logical[A], Arithmetic[A]
  fun box shl(y: A): A
  fun box shr(y: A): A

trait ArithmeticConvertible
  fun box i8(): I8 => compiler_intrinsic
  fun box i16(): I16 => compiler_intrinsic
  fun box i32(): I32 => compiler_intrinsic
  fun box i64(): I64 => compiler_intrinsic
  fun box i128(): I128 => compiler_intrinsic

  fun box u8(): U8 => compiler_intrinsic
  fun box u16(): U16 => compiler_intrinsic
  fun box u32(): U32 => compiler_intrinsic
  fun box u64(): U64 => compiler_intrinsic
  fun box u128(): U128 => compiler_intrinsic

  fun box f32(): F32 => compiler_intrinsic
  fun box f64(): F64 => compiler_intrinsic

trait Real[A: Real[A]] is Stringable, ArithmeticConvertible, Arithmetic[A],
  Hashable, Ordered[A]
  fun box add(y: A): A => compiler_intrinsic
  fun box sub(y: A): A => compiler_intrinsic
  fun box mul(y: A): A => compiler_intrinsic
  fun box div(y: A): A => compiler_intrinsic
  fun box mod(y: A): A => compiler_intrinsic
  fun box neg(): A => compiler_intrinsic
  fun box eq(y: A box): Bool => compiler_intrinsic
  fun box ne(y: A box): Bool => compiler_intrinsic
  fun box lt(y: A box): Bool => compiler_intrinsic
  fun box le(y: A box): Bool => compiler_intrinsic
  fun box ge(y: A box): Bool => compiler_intrinsic
  fun box gt(y: A box): Bool => compiler_intrinsic

  fun box hash(): U64 =>
    var x = u64()
    x = (not x) + (x << 21);
    x = x xor (x >> 24);
    x = (x + (x << 3)) + (x << 8);
    x = x xor (x >> 14);
    x = (x + (x << 2)) + (x << 4);
    x = x xor (x >> 28);
    x = x + (x << 31);
    x

trait Integer[A: Integer[A]] is Real[A], Logical[A], Bits[A]
  fun box and_(y: A): A => compiler_intrinsic
  fun box or_(y: A): A => compiler_intrinsic
  fun box xor_(y: A): A => compiler_intrinsic
  fun box not_(): A => compiler_intrinsic
  fun box shl(y: A): A => compiler_intrinsic
  fun box shr(y: A): A => compiler_intrinsic

  fun box abs(): A
  fun box bswap(): A
  fun box popcount(): A
  fun box clz(): A

type Number is (Signed | Unsigned | Float)

type Signed is (I8 | I16 | I32 | I64 | I128)

type Unsigned is (U8 | U16 | U32 | U64 | U128)

type Float is (F32 | F64)
