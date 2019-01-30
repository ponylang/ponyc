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

  fun i8_unsafe(): I8 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun i16_unsafe(): I16 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun i32_unsafe(): I32 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun i64_unsafe(): I64 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun i128_unsafe(): I128 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun ilong_unsafe(): ILong =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun isize_unsafe(): ISize =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun u8_unsafe(): U8 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun u16_unsafe(): U16 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun u32_unsafe(): U32 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun u64_unsafe(): U64 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun u128_unsafe(): U128 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun ulong_unsafe(): ULong =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun usize_unsafe(): USize =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun f32_unsafe(): F32 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

  fun f64_unsafe(): F64 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    compile_intrinsic

trait val Real[A: Real[A] val] is
  (Stringable & _ArithmeticConvertible & Comparable[A])
  new val create(value: A)

  new val from[B: (Number & Real[B] val)](a: B)
  new val min_value()
  new val max_value()

  fun add(y: A): A => this + y
  fun sub(y: A): A => this - y
  fun mul(y: A): A => this * y
  fun div(y: A): A =>
    """
    Integer division, rounded towards zero.
    """
    this / y

  fun divrem(y: A): (A, A) => (this / y, this % y)
  fun rem(y: A): A =>
    """
    Calculate the remainder after integer division, rounded towards zero (`div`).

    The result has the sign of the dividend.
    """
    this % y

  fun neg(): A => -this

  fun fld(y: A): A
    """
    Floored integer division, rounded towards negative infinity.
    """
  fun mod(y: A): A
    """
    Calculate the modulo after floored integer division, rounded towards negative infinity (`fld`).

    The result has the sign of the divisor.
    """

  fun eq(y: box->A): Bool => this == y
  fun ne(y: box->A): Bool => this != y
  fun lt(y: box->A): Bool => this < y
  fun le(y: box->A): Bool => this <= y
  fun ge(y: box->A): Bool => this >= y
  fun gt(y: box->A): Bool => this > y

  fun min(y: A): A
  fun max(y: A): A

  fun hash(): USize =>
    var x = usize()

    ifdef ilp32 then
      x = (not x) + (x << 15)
      x = x xor (x >> 12)
      x = x + (x << 2)
      x = x xor (x >> 4)
      x = (x + (x << 3)) + (x << 11)
      x = x xor (x >> 16)

      x
    else
      hash64().usize()
    end

  fun hash64(): U64 =>
    var x = u64()

    x = (not x) + (x << 21)
    x = x xor (x >> 24)
    x = (x + (x << 3)) + (x << 8)
    x = x xor (x >> 14)
    x = (x + (x << 2)) + (x << 4)
    x = x xor (x >> 28)
    x = x + (x << 31)

    x

  fun _value(): A => compile_intrinsic

trait val Integer[A: Integer[A] val] is Real[A]
  fun add_unsafe(y: A): A =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    this +~ y

  fun sub_unsafe(y: A): A =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    this -~ y

  fun mul_unsafe(y: A): A =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    this *~ y

  fun div_unsafe(y: A): A =>
    """
    Integer division, rounded towards zero.

    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    this /~ y

  fun divrem_unsafe(y: A): (A, A) =>
    """
    Calculates the quotient of this number and `y` and the remainder.

    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    (this /~ y, this %~ y)

  fun rem_unsafe(y: A): A =>
    """
    Calculates the remainder of this number divided by `y`.

    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    this %~ y

  fun fld_unsafe(y: A): A
    """
    Floored division, rounded towards negative infinity,
    as opposed to `div` which rounds towards zero.

    *Unsafe Operation*

    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """

  fun mod_unsafe(y: A): A
    """
    Calculates the modulo of this number after floored division by `y`.

    *Unsafe Operation.*

    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """

  fun add_partial(y: A): A ?
    """
    Add y to this number.

    If the operation overflows this function errors.
    """

  fun sub_partial(y: A): A ?
    """
    Subtract y from this number.

    If the operation overflows/underflows this function errors.
    """

  fun mul_partial(y: A): A ?
    """
    Multiply y with this number.

    If the operation overflows this function errors.
    """

  fun div_partial(y: A): A ?
    """
    Divides this number by `y`, rounds the result towards zero.

    If y is `0` or the operation overflows, this function errors.
    """

  fun rem_partial(y: A): A ?
    """
    Calculates the remainder of this number divided by y.
    The result has the sign of the dividend.

    If y is `0` or the operation overflows, this function errors.
    """

  fun divrem_partial(y: A): (A, A) ?
    """
    Divides this number by y and calculates the remainder of the operation.

    If y is `0` or the operation overflows, this function errors.
    """

  fun fld_partial(y: A): A ?
    """
    Floored integer division, rounded towards negative infinity.

    If y is `0` or the operation overflows, this function errors
    """

  fun mod_partial(y: A): A ?
    """
    Calculates the modulo of this number and `y` after floored division (`fld`).
    The result has the sign of the divisor.

    If y is `0` or the operation overflows, this function errors.
    """

  fun neg_unsafe(): A =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    -~this

  fun addc(y: A): (A, Bool)
    """
    Add `y` to this integer and return the result and a flag indicating overflow.
    """
  fun subc(y: A): (A, Bool)
    """
    Subtract `y` from this integer and return the result and a flag indicating overflow.
    """
  fun mulc(y: A): (A, Bool)
    """
    Multiply `y` with this integer and return the result and a flag indicating overflow.
    """
  fun divc(y: A): (A, Bool)
    """
    Divide this integer by `y` and return the result and a flag indicating overflow or division by zero.
    """
  fun remc(y: A): (A, Bool)
    """
    Calculate the remainder of this number divided by `y` and return the result and a flag indicating division by zero or overflow.

    The result will have the sign of the dividend.
    """
  fun fldc(y: A): (A, Bool)
    """
    Divide this integer by `y` and return the result, rounded towards negative infinity and a flag indicating overflow or division by zero.
    """
  fun modc(y: A): (A, Bool)
    """
    Calculate the modulo of this number after floored division by `y` and return the result and a flag indicating division by zero or overflow.

    The result will have the sign of the divisor.
    """

  fun op_and(y: A): A => this and y
  fun op_or(y: A): A => this or y
  fun op_xor(y: A): A => this xor y
  fun op_not(): A => not this

  fun bit_reverse(): A
    """
    Reverse the order of the bits within the integer.
    For example, 0b11101101 (237) would return 0b10110111 (183).
    """

  fun bswap(): A

trait val SignedInteger[A: SignedInteger[A, B] val,
    B: UnsignedInteger[B] val] is Integer[A]
  fun abs(): B

  fun shl(y: B): A => this << y
  fun shr(y: B): A => this >> y

  fun shl_unsafe(y: B): A =>
    """
    Unsafe operation.
    If bits differing from the final sign bit are shifted-out, the result is
    undefined.
    """
    this <<~ y

  fun shr_unsafe(y: B): A =>
    """
    Unsafe operation.
    If non-zero bits are shifted-out, the result is undefined.
    """
    this >>~ y

  fun popcount(): B
  fun clz(): B
  fun ctz(): B

  fun clz_unsafe(): B
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """

  fun ctz_unsafe(): B
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """

  fun bitwidth(): B

  fun bytewidth(): USize

  fun string(): String iso^ =>
    _ToString._u64(abs().u64(), i64() < 0)

trait val UnsignedInteger[A: UnsignedInteger[A] val] is Integer[A]
  fun abs(): A

  fun shl(y: A): A => this << y
  fun shr(y: A): A => this >> y

  // both fld and mod behave the same as div and rem for unsigned integers
  fun fld(y: A): A => this / y
  fun fldc(y: A): (A, Bool) => this.divc(y)
  fun fld_partial(y: A): A ? => this.div_partial(y)?
  fun fld_unsafe(y: A): A => this.div_unsafe(y)

  fun mod(y: A): A => this % y
  fun modc(y: A): (A, Bool) => this.remc(y)
  fun mod_partial(y: A): A ? => this.rem_partial(y)?
  fun mod_unsafe(y: A): A => this.rem_unsafe(y)

  fun shl_unsafe(y: A): A =>
    """
    Unsafe operation.
    If non-zero bits are shifted-out, the result is undefined.
    """
    this <<~ y

  fun shr_unsafe(y: A): A =>
    """
    Unsafe operation.
    If non-zero bits are shifted-out, the result is undefined.
    """
    this >>~ y

  fun rotl(y: A): A =>
    let y' = y % bitwidth()
    if y' == y'.min_value() then
      this._value()
    else
      (this << y') or (this >> (bitwidth() - y'))
    end

  fun rotr(y: A): A =>
    let y' = y % bitwidth()
    if y' == y'.min_value() then
      this._value()
    else
      (this >> y') or (this << (bitwidth() - y'))
    end

  fun popcount(): A
  fun clz(): A
  fun ctz(): A

  fun clz_unsafe(): A
    """
    Count leading zeroes.

    Unsafe operation.
    If this is 0, the result is undefined.
    """

  fun ctz_unsafe(): A
    """
    Count trailing zeroes.

    Unsafe operation.
    If this is 0, the result is undefined.
    """

  fun bitwidth(): A

  fun bytewidth(): USize

  fun string(): String iso^ =>
    _ToString._u64(u64(), false)

trait val FloatingPoint[A: FloatingPoint[A] val] is Real[A]
  new val min_normalised()
  new val epsilon()
  fun tag radix(): U8
  fun tag precision2(): U8
  fun tag precision10(): U8
  fun tag min_exp2(): I16
  fun tag min_exp10(): I16
  fun tag max_exp2(): I16
  fun tag max_exp10(): I16

  fun add_unsafe(y: A): A =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this +~ y

  fun sub_unsafe(y: A): A =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this -~ y

  fun mul_unsafe(y: A): A =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this *~ y

  fun div_unsafe(y: A): A =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this /~ y

  fun fld_unsafe(y: A): A
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """

  fun divrem_unsafe(y: A): (A, A) =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    (this /~ y, this %~ y)

  fun rem_unsafe(y: A): A =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this %~ y

  fun mod_unsafe(y: A): A
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """

  fun neg_unsafe(): A =>
    """
    Unsafe operation.
    If any input or output of the operation is +/- infinity or NaN, the result
    is undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    -~this

  fun eq_unsafe(y: box->A): Bool =>
    """
    Unsafe operation.
    If any input of the operation is +/- infinity or NaN, the result is
    undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this ==~ y

  fun ne_unsafe(y: box->A): Bool =>
    """
    Unsafe operation.
    If any input of the operation is +/- infinity or NaN, the result is
    undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this !=~ y

  fun lt_unsafe(y: box->A): Bool =>
    """
    Unsafe operation.
    If any input of the operation is +/- infinity or NaN, the result is
    undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this <~ y

  fun le_unsafe(y: box->A): Bool =>
    """
    Unsafe operation.
    If any input of the operation is +/- infinity or NaN, the result is
    undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this <=~ y

  fun ge_unsafe(y: box->A): Bool =>
    """
    Unsafe operation.
    If any input of the operation is +/- infinity or NaN, the result is
    undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this >=~ y

  fun gt_unsafe(y: box->A): Bool =>
    """
    Unsafe operation.
    If any input of the operation is +/- infinity or NaN, the result is
    undefined.
    The operation isn't required to fully comply to IEEE 754 semantics.
    """
    this >~ y

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

  fun sqrt_unsafe(): A
    """
    Unsafe operation.
    If this is negative, the result is undefined.
    """

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

  fun string(): String iso^ =>
    _ToString._f64(f64())

type Number is (Int | Float)

type Int is (Signed | Unsigned)
