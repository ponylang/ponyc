trait val _ArithmeticConvertible
  fun i8(): I8 => compile_intrinsic
  fun i16(): I16 => compile_intrinsic
  fun i32(): I32 => compile_intrinsic
  fun i64(): I64 => compile_intrinsic
  fun i128(): I128 => compile_intrinsic
  fun ilong(): ILong => compile_intrinsic
  fun isize(): ISize => compile_intrinsic

  fun u8(): U8 => compile_intrinsic
  fun u16(): U16 => compile_intrinsic
  fun u32(): U32 => compile_intrinsic
  fun u64(): U64 => compile_intrinsic
  fun u128(): U128 => compile_intrinsic
  fun ulong(): ULong => compile_intrinsic
  fun usize(): USize => compile_intrinsic

  fun f32(): F32 => compile_intrinsic
  fun f64(): F64 => compile_intrinsic

trait val Real[A: Real[A] val] is
  (Stringable & _ArithmeticConvertible & Comparable[A])
  new val create(value: A)

  fun tag from[B: (Number & Real[B] val)](a: B): A
  fun tag min_value(): A
  fun tag max_value(): A

  fun add(y: A): A => this + y
  fun sub(y: A): A => this - y
  fun mul(y: A): A => this * y
  fun div(y: A): A => this / y
  fun divmod(y: A): (A, A) => (this / y, this % y)
  fun mod(y: A): A => this % y
  fun neg(): A => -this
  fun eq(y: box->A): Bool => this == y
  fun ne(y: box->A): Bool => this != y
  fun lt(y: box->A): Bool => this < y
  fun le(y: box->A): Bool => this <= y
  fun ge(y: box->A): Bool => this >= y
  fun gt(y: box->A): Bool => this > y

  fun min(y: A): A
  fun max(y: A): A

  fun hash(): U64 =>
    var x = u64()
    x = (not x) + (x << 21)
    x = x xor (x >> 24)
    x = (x + (x << 3)) + (x << 8)
    x = x xor (x >> 14)
    x = (x + (x << 2)) + (x << 4)
    x = x xor (x >> 28)
    x = x + (x << 31)
    x

trait val Integer[A: Integer[A] val] is Real[A]
  fun op_and(y: A): A => this and y
  fun op_or(y: A): A => this or y
  fun op_xor(y: A): A => this xor y
  fun op_not(): A => not this
  fun shl(y: A): A => this << y
  fun shr(y: A): A => this >> y

  fun bswap(): A
  fun popcount(): A
  fun clz(): A
  fun ctz(): A
  fun bitwidth(): A

  fun rotl(y: A): A => (this << y) or (this >> (bitwidth() - y))
  fun rotr(y: A): A => (this >> y) or (this << (bitwidth() - y))

trait val _SignedInteger[A: _SignedInteger[A, B] val,
    B: _UnsignedInteger[B] val] is Integer[A]
  fun abs(): B
  fun string(
    fmt: FormatSettings[FormatInt, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    _ToString._u64(abs().u64(), i64() < 0, fmt)

trait val _UnsignedInteger[A: _UnsignedInteger[A] val] is Integer[A]
  fun abs(): A
  fun string(
    fmt: FormatSettings[FormatInt, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    _ToString._u64(u64(), false, fmt)

trait val FloatingPoint[A: FloatingPoint[A] val] is Real[A]
  fun tag epsilon(): A
  fun tag radix(): U8
  fun tag precision2(): U8
  fun tag precision10(): U8
  fun tag min_exp2(): I16
  fun tag min_exp10(): I16
  fun tag max_exp2(): I16
  fun tag max_exp10(): I16

  fun abs(): A
  fun ceil(): A
  fun floor(): A
  fun round(): A
  fun trunc(): A

  fun finite(): Bool
  fun infinite(): Bool
  fun nan(): Bool

  fun ldexp(x: A, exponent: I32): A
  fun frexp(): (A, U32)
  fun log(): A
  fun log2(): A
  fun log10(): A
  fun logb(): A

  fun pow(y: A): A
  fun powi(y: I32): A

  fun sqrt(): A
  fun cbrt(): A
  fun exp(): A
  fun exp2(): A

  fun cos(): A
  fun sin(): A
  fun tan(): A

  fun cosh(): A
  fun sinh(): A
  fun tanh(): A

  fun acos(): A
  fun asin(): A
  fun atan(): A
  fun atan2(y: A): A

  fun acosh(): A
  fun asinh(): A
  fun atanh(): A

  fun copysign(sign: A): A

  fun string(
    fmt: FormatSettings[FormatFloat, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    _ToString._f64(f64(), fmt)

type Number is (Signed | Unsigned | Float)

type Signed is (I8 | I16 | I32 | I64 | I128 | ILong | ISize)

type Unsigned is (U8 | U16 | U32 | U64 | U128 | ULong | USize)

type Float is (F32 | F64)
