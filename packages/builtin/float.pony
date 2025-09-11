use @"llvm.fabs.f32"[F32](x: F32)
use @"llvm.fabs.f64"[F64](x: F64)
use @"llvm.ceil.f32"[F32](x: F32)
use @"llvm.ceil.f64"[F64](x: F64)
use @"llvm.floor.f32"[F32](x: F32)
use @"llvm.floor.f64"[F64](x: F64)
use @"llvm.round.f32"[F32](x: F32)
use @"llvm.round.f64"[F64](x: F64)
use @"llvm.trunc.f32"[F32](x: F32)
use @"llvm.trunc.f64"[F64](x: F64)
use @"llvm.log.f32"[F32](x: F32)
use @"llvm.log.f64"[F64](x: F64)
use @"llvm.log2.f32"[F32](x: F32)
use @"llvm.log2.f64"[F64](x: F64)
use @"llvm.log10.f32"[F32](x: F32)
use @"llvm.log10.f64"[F64](x: F64)
use @logbf[F32](x: F32)
use @logb[F64](x: F64)
use @"llvm.pow.f32"[F32](x: F32, y: F32)
use @"llvm.pow.f64"[F64](x: F64, y: F64)
use @"llvm.powi.f32.i32"[F32](x: F32, y: I32) if not windows
use @"llvm.powi.f64.i32"[F64](x: F64, y: I32) if not windows
use @"llvm.sqrt.f32"[F32](x: F32)
use @"llvm.sqrt.f64"[F64](x: F64)
use @cbrtf[F32](x: F32)
use @cbrt[F64](x: F64)
use @"llvm.exp.f32"[F32](x: F32)
use @"llvm.exp.f64"[F64](x: F64)
use @"llvm.exp2.f32"[F32](x: F32)
use @"llvm.exp2.f64"[F64](x: F64)
use @"llvm.cos.f32"[F32](x: F32)
use @"llvm.cos.f64"[F64](x: F64)
use @"llvm.sin.f32"[F32](x: F32)
use @"llvm.sin.f64"[F64](x: F64)
use @tanf[F32](x: F32)
use @coshf[F32](x: F32)
use @sinhf[F32](x: F32)
use @tanhf[F32](x: F32)
use @acosf[F32](x: F32)
use @asinf[F32](x: F32)
use @atanf[F32](x: F32)
use @atan2f[F32](x: F32, y: F32)
use @acoshf[F32](x: F32)
use @asinhf[F32](x: F32)
use @atanhf[F32](x: F32)
use @tan[F64](x: F64)
use @cosh[F64](x: F64)
use @sinh[F64](x: F64)
use @tanh[F64](x: F64)
use @acos[F64](x: F64)
use @asin[F64](x: F64)
use @atan[F64](x: F64)
use @atan2[F64](x: F64, y: F64)
use @acosh[F64](x: F64)
use @asinh[F64](x: F64)
use @atanh[F64](x: F64)
use @"llvm.copysign.f32"[F32](x: F32, sign: F32)
use @"llvm.copysign.f64"[F64](x: F64, sign: F64)
use @frexp[F64](value: F64, exponent: Pointer[U32])
use @ldexpf[F32](value: F32, exponent: I32)
use @ldexp[F64](value: F64, exponent: I32)

primitive F32 is FloatingPoint[F32]
  new create(value: F32 = 0) => value
  new pi() => 3.14159265358979323846
  new e() => 2.71828182845904523536

  new _nan() => compile_intrinsic
  new _inf(negative: Bool) => compile_intrinsic

  new from_bits(i: U32) => compile_intrinsic
  fun bits(): U32 => compile_intrinsic
  new from[B: (Number & Real[B] val)](a: B) => a.f32()

  new min_value() =>
    """
    Minimum negative value representable.
    """
    from_bits(0xFF7FFFFF)

  new max_value() =>
    """
    Maximum positive value representable.
    """
    from_bits(0x7F7FFFFF)

  new min_normalised() =>
    """
    Minimum positive value representable at full precision (ie a normalised
    number).
    """
    from_bits(0x00800000)

  new epsilon() =>
    """
    Minimum positive value such that (1 + epsilon) != 1.
    """
    from_bits(0x34000000)

  fun tag radix(): U8 =>
    """
    Exponent radix.
    """
    2

  fun tag precision2(): U8 =>
    """
    Mantissa precision in bits.
    """
    24

  fun tag precision10(): U8 =>
    """
    Mantissa precision in decimal digits.
    """
    6

  fun tag min_exp2(): I16 =>
    """
    Minimum exponent value such that (2^exponent) - 1 is representable at full
    precision (ie a normalised number).
    """
    -125

  fun tag min_exp10(): I16 =>
    """
    Minimum exponent value such that (10^exponent) - 1 is representable at full
    precision (ie a normalised number).
    """
    -37

  fun tag max_exp2(): I16 =>
    """
    Maximum exponent value such that (2^exponent) - 1 is representable.
    """
    128

  fun tag max_exp10(): I16 =>
    """
    Maximum exponent value such that (10^exponent) - 1 is representable.
    """
    38

  fun abs(): F32 => @"llvm.fabs.f32"(this)
  fun ceil(): F32 => @"llvm.ceil.f32"(this)
  fun floor(): F32 => @"llvm.floor.f32"(this)
  fun round(): F32 => @"llvm.round.f32"(this)
  fun trunc(): F32 => @"llvm.trunc.f32"(this)

  fun min(y: F32): F32 => if this < y then this else y end
  fun max(y: F32): F32 => if this > y then this else y end

  fun fld(y: F32): F32 =>
    (this / y).floor()

  fun fld_unsafe(y: F32): F32 =>
    (this /~ y).floor()

  fun mod(y: F32): F32 =>
    let r = this % y
    if r == F32(0.0) then
      r.copysign(y)
    elseif (r > 0) xor (y > 0) then
      r + y
    else
      r
    end

  fun mod_unsafe(y: F32): F32 =>
    let r = this %~ y
    if r == F32(0.0) then
      r.copysign(y)
    elseif (r > 0) xor (y > 0) then
      r + y
    else
      r
    end

  fun finite(): Bool =>
    """
    Check whether this number is finite, ie not +/-infinity and not NaN.
    """
    // True if exponent is not all 1s
    (bits() and 0x7F800000) != 0x7F800000

  fun infinite(): Bool =>
    """
    Check whether this number is +/-infinity
    """
    // True if exponent is all 1s and mantissa is 0
    ((bits() and 0x7F800000) == 0x7F800000) and  // exp
    ((bits() and 0x007FFFFF) == 0)  // mantissa

  fun nan(): Bool =>
    """
    Check whether this number is NaN.
    """
    // True if exponent is all 1s and mantissa is non-0
    ((bits() and 0x7F800000) == 0x7F800000) and  // exp
    ((bits() and 0x007FFFFF) != 0)  // mantissa

  fun ldexp(x: F32, exponent: I32): F32 =>
    @ldexpf(x, exponent)

  fun frexp(): (F32, U32) =>
    var exponent: U32 = 0
    var mantissa = @frexp(f64(), addressof exponent)
    (mantissa.f32(), exponent)

  fun log(): F32 => @"llvm.log.f32"(this)
  fun log2(): F32 => @"llvm.log2.f32"(this)
  fun log10(): F32 => @"llvm.log10.f32"(this)
  fun logb(): F32 => @logbf(this)

  fun pow(y: F32): F32 => @"llvm.pow.f32"(this, y)
  fun powi(y: I32): F32 =>
    ifdef windows then
      pow(y.f32())
    else
      @"llvm.powi.f32.i32"(this, y)
    end

  fun sqrt(): F32 =>
    if this < 0.0 then
      _nan()
    else
      @"llvm.sqrt.f32"(this)
    end

  fun sqrt_unsafe(): F32 =>
    """
    Unsafe operation.
    If this is negative, the result is undefined.
    """
    @"llvm.sqrt.f32"(this)

  fun cbrt(): F32 => @cbrtf(this)
  fun exp(): F32 => @"llvm.exp.f32"(this)
  fun exp2(): F32 => @"llvm.exp2.f32"(this)

  fun cos(): F32 => @"llvm.cos.f32"(this)
  fun sin(): F32 => @"llvm.sin.f32"(this)
  fun tan(): F32 => @tanf(this)

  fun cosh(): F32 => @coshf(this)
  fun sinh(): F32 => @sinhf(this)
  fun tanh(): F32 => @tanhf(this)

  fun acos(): F32 => @acosf(this)
  fun asin(): F32 => @asinf(this)
  fun atan(): F32 => @atanf(this)
  fun atan2(y: F32): F32 => @atan2f(this, y)

  fun acosh(): F32 => @acoshf(this)
  fun asinh(): F32 => @asinhf(this)
  fun atanh(): F32 => @atanhf(this)

  fun copysign(sign: F32): F32 => @"llvm.copysign.f32"(this, sign)

  fun hash(): USize => bits().hash()
  fun hash64(): U64 => bits().hash64()

  fun i128(): I128 => f64().i128()
  fun u128(): U128 => f64().u128()

  fun i128_unsafe(): I128 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    f64_unsafe().i128_unsafe()

  fun u128_unsafe(): U128 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    f64_unsafe().u128_unsafe()

primitive F64 is FloatingPoint[F64]
  new create(value: F64 = 0) => value
  new pi() => 3.14159265358979323846
  new e() => 2.71828182845904523536

  new _nan() => compile_intrinsic
  new _inf(negative: Bool) => compile_intrinsic

  new from_bits(i: U64) => compile_intrinsic
  fun bits(): U64 => compile_intrinsic
  new from[B: (Number & Real[B] val)](a: B) => a.f64()

  new min_value() =>
    """
    Minimum negative value representable.
    """
    from_bits(0xFFEF_FFFF_FFFF_FFFF)

  new max_value() =>
    """
    Maximum positive value representable.
    """
    from_bits(0x7FEF_FFFF_FFFF_FFFF)

  new min_normalised() =>
    """
    Minimum positive value representable at full precision (ie a normalised
    number).
    """
    from_bits(0x0010_0000_0000_0000)

  new epsilon() =>
    """
    Minimum positive value such that (1 + epsilon) != 1.
    """
    from_bits(0x3CB0_0000_0000_0000)

  fun tag radix(): U8 =>
    """
    Exponent radix.
    """
    2

  fun tag precision2(): U8 =>
    """
    Mantissa precision in bits.
    """
    53

  fun tag precision10(): U8 =>
    """
    Mantissa precision in decimal digits.
    """
    15

  fun tag min_exp2(): I16 =>
    """
    Minimum exponent value such that (2^exponent) - 1 is representable at full
    precision (ie a normalised number).
    """
    -1021

  fun tag min_exp10(): I16 =>
    """
    Minimum exponent value such that (10^exponent) - 1 is representable at full
    precision (ie a normalised number).
    """
    -307

  fun tag max_exp2(): I16 =>
    """
    Maximum exponent value such that (2^exponent) - 1 is representable.
    """
    1024

  fun tag max_exp10(): I16 =>
    """
    Maximum exponent value such that (10^exponent) - 1 is representable.
    """
    308

  fun abs(): F64 => @"llvm.fabs.f64"(this)
  fun ceil(): F64 => @"llvm.ceil.f64"(this)
  fun floor(): F64 => @"llvm.floor.f64"(this)
  fun round(): F64 => @"llvm.round.f64"(this)
  fun trunc(): F64 => @"llvm.trunc.f64"(this)

  fun min(y: F64): F64 => if this < y then this else y end
  fun max(y: F64): F64 => if this > y then this else y end

  fun fld(y: F64): F64 =>
    (this / y).floor()

  fun fld_unsafe(y: F64): F64 =>
    (this /~ y).floor()

  fun mod(y: F64): F64 =>
    let r = this.rem(y)
    if r == F64(0.0) then
      r.copysign(y)
    elseif (r > 0) xor (y > 0) then
      r + y
    else
      r
    end

  fun mod_unsafe(y: F64): F64 =>
    let r = this %~ y
    if r == F64(0.0) then
      r.copysign(y)
    elseif (r > 0) xor (y > 0) then
      r + y
    else
      r
    end

  fun finite(): Bool =>
    """
    Check whether this number is finite, ie not +/-infinity and not NaN.
    """
    // True if exponent is not all 1s
    (bits() and 0x7FF0_0000_0000_0000) != 0x7FF0_0000_0000_0000

  fun infinite(): Bool =>
    """
    Check whether this number is +/-infinity
    """
    // True if exponent is all 1s and mantissa is 0
    ((bits() and 0x7FF0_0000_0000_0000) == 0x7FF0_0000_0000_0000) and  // exp
    ((bits() and 0x000F_FFFF_FFFF_FFFF) == 0)  // mantissa

  fun nan(): Bool =>
    """
    Check whether this number is NaN.
    """
    // True if exponent is all 1s and mantissa is non-0
    ((bits() and 0x7FF0_0000_0000_0000) == 0x7FF0_0000_0000_0000) and  // exp
    ((bits() and 0x000F_FFFF_FFFF_FFFF) != 0)  // mantissa

  fun ldexp(x: F64, exponent: I32): F64 =>
    @ldexp(x, exponent)

  fun frexp(): (F64, U32) =>
    var exponent: U32 = 0
    var mantissa = @frexp(this, addressof exponent)
    (mantissa, exponent)

  fun log(): F64 => @"llvm.log.f64"(this)
  fun log2(): F64 => @"llvm.log2.f64"(this)
  fun log10(): F64 => @"llvm.log10.f64"(this)
  fun logb(): F64 => @logb(this)

  fun pow(y: F64): F64 => @"llvm.pow.f64"(this, y)
  fun powi(y: I32): F64 =>
    ifdef windows then
      pow(y.f64())
    else
      @"llvm.powi.f64.i32"(this, y)
    end

  fun sqrt(): F64 =>
    if this < 0.0 then
      _nan()
    else
      @"llvm.sqrt.f64"(this)
    end

  fun sqrt_unsafe(): F64 =>
    """
    Unsafe operation.
    If this is negative, the result is undefined.
    """
    @"llvm.sqrt.f64"(this)

  fun cbrt(): F64 => @cbrt(this)
  fun exp(): F64 => @"llvm.exp.f64"(this)
  fun exp2(): F64 => @"llvm.exp2.f64"(this)

  fun cos(): F64 => @"llvm.cos.f64"(this)
  fun sin(): F64 => @"llvm.sin.f64"(this)
  fun tan(): F64 => @tan(this)

  fun cosh(): F64 => @cosh(this)
  fun sinh(): F64 => @sinh(this)
  fun tanh(): F64 => @tanh(this)

  fun acos(): F64 => @acos(this)
  fun asin(): F64 => @asin(this)
  fun atan(): F64 => @atan(this)
  fun atan2(y: F64): F64 => @atan2(this, y)

  fun acosh(): F64 => @acosh(this)
  fun asinh(): F64 => @asinh(this)
  fun atanh(): F64 => @atanh(this)

  fun copysign(sign: F64): F64 => @"llvm.copysign.f64"(this, sign)

  fun hash(): USize => bits().hash()
  fun hash64(): U64 => bits().hash64()

  fun i128(): I128 =>
    if this > I128.max_value().f64() then
      return I128.max_value()
    elseif this < I128.min_value().f64() then
      return I128.min_value()
    end

    let bit = bits()
    let high = (bit >> 32).u32()
    let ex = ((high and 0x7FF00000) >> 20) - 1023

    if ex < 0 then
      return 0
    end

    let s = ((high and 0x80000000) >> 31).i128()
    var r = ((bit and 0x000FFFFFFFFFFFFF) or 0x0010000000000000).i128()
    let ex' = ex.u128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (52 - ex')
    end

    (r xor s) - s

  fun u128(): U128 =>
    if this > U128.max_value().f64() then
      return U128.max_value()
    elseif this < U128.min_value().f64() then
      return U128.min_value()
    end

    let bit = bits()
    let high = (bit >> 32).u32()
    let ex = ((high and 0x7FF00000) >> 20) - 1023

    if (ex < 0) or ((high and 0x80000000) != 0) then
      return 0
    end

    var r = ((bit and 0x000FFFFFFFFFFFFF) or 0x0010000000000000).u128()
    let ex' = ex.u128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (52 - ex')
    end

    r.u128()

  fun i128_unsafe(): I128 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    i128()

  fun u128_unsafe(): U128 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    u128()

type Float is (F32 | F64)
