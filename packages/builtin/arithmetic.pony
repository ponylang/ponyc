interface Arithmetic[A: Arithmetic[A] box]
  fun box add(y: A): A
  fun box sub(y: A): A
  fun box mul(y: A): A
  fun box div(y: A): A
  fun box mod(y: A): A
  fun box divmod(y: A): (A, A) => (this / y, this % y)
  fun box neg(): A

interface Logical[A: Logical[A] box]
  fun box op_and(y: A): A
  fun box op_or(y: A): A
  fun box op_xor(y: A): A
  fun box op_not(): A

interface Bits[A: Bits[A] box] is Logical[A]
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

trait Real[A: Real[A] box] is Stringable, ArithmeticConvertible, Arithmetic[A],
  Ordered[A]
  fun box add(y: A): A => this + y
  fun box sub(y: A): A => this - y
  fun box mul(y: A): A => this * y
  fun box div(y: A): A => this / y
  fun box mod(y: A): A => this % y
  fun box neg(): A => -this
  fun box eq(y: A): Bool => this == y
  fun box ne(y: A): Bool => this != y
  fun box lt(y: A): Bool => this < y
  fun box le(y: A): Bool => this <= y
  fun box ge(y: A): Bool => this >= y
  fun box gt(y: A): Bool => this > y

  fun box abs(): A
  fun box max(that: A): A
  fun box min(that: A): A

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

trait Integer[A: Integer[A] box] is Real[A], Logical[A], Bits[A]
  fun box op_and(y: A): A => this and y
  fun box op_or(y: A): A => this or y
  fun box op_xor(y: A): A => this xor y
  fun box op_not(): A => not this
  fun box shl(y: A): A => this << y
  fun box shr(y: A): A => this >> y

  fun box bswap(): A
  fun box popcount(): A
  fun box clz(): A
  fun box ctz(): A
  fun box bitwidth(): A

trait SignedInteger[A: SignedInteger[A] box] is Integer[A]
  fun box string(fmt: IntFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = 1, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    ToString._u64(abs().u64(), this.i64() < 0, fmt, prefix, prec, width, align,
      fill)

trait UnsignedInteger[A: UnsignedInteger[A] box] is Integer[A]
  fun box string(fmt: IntFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = 1, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    ToString._u64(u64(), false, fmt, prefix, prec, width, align, fill)

trait FloatingPoint[A: FloatingPoint[A] box] is Real[A]
  fun box string(fmt: FloatFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = 6, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    ToString._f64(f64(), fmt, prefix, prec, width, align, fill)

type Number is (Signed | Unsigned | Float)

type Signed is (I8 | I16 | I32 | I64 | I128)

type Unsigned is (U8 | U16 | U32 | U64 | U128)

type Float is (F32 | F64)
