primitive F32 is FloatingPoint[F32]
  new create(from: F32) => compiler_intrinsic
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U32) => compiler_intrinsic
  fun bits(): U32 => compiler_intrinsic

  fun abs(): F32 => @"llvm.fabs.f32"[F32](this)
  fun min(y: F32): F32 => if this < y then this else y end
    //@"llvm.minnum.f32"[F32](this, y)
  fun max(y: F32): F32 => if this > y then this else y end
    //@"llvm.maxnum.f32"[F32](this, y)

  fun ceil(): F32 => @"llvm.ceil.f32"[F32](this)
  fun floor(): F32 => @"llvm.floor.f32"[F32](this)
  fun round(): F32 => @"llvm.round.f32"[F32](this)
  fun trunc(): F32 => @"llvm.trunc.f32"[F32](this)

  fun finite(): Bool =>
    if Platform.windows() then
      @_finite[I32](this.f64()) != 0
    elseif Platform.osx() then
      @finite[I32](this.f64()) != 0
    else
      @finitef[I32](this) != 0
    end

  fun nan(): Bool =>
    if Platform.windows() then
      @_isnan[I32](this.f64()) != 0
    elseif Platform.osx() then
      @isnan[I32](this.f64()) != 0
    else
      @isnanf[I32](this) != 0
    end

  fun frexp(): (F32, U32) =>
    var exponent: U32 = 0
    var mantissa = @frexp[F64](f64(), &exponent)
    (mantissa.f32(), exponent)

  fun log(): F32 => @"llvm.log.f32"[F32](this)
  fun log2(): F32 => @"llvm.log2.f32"[F32](this)
  fun log10(): F32 => @"llvm.log10.f32"[F32](this)
  fun logb(): F32 => @logbf[F32](this)

  fun pow(y: F32): F32 => @"llvm.pow.f32"[F32](this, y)
  fun powi(y: I32): F32 =>
    if Platform.windows() then
      pow(y.f32())
    else
      @"llvm.powi.f32"[F32](this, y)
    end

  fun sqrt(): F32 => @"llvm.sqrt.f32"[F32](this)
  fun cbrt(): F32 => @cbrtf[F32](this)
  fun exp(): F32 => @"llvm.exp.f32"[F32](this)
  fun exp2(): F32 => @"llvm.exp2.f32"[F32](this)

  fun cos(): F32 => @"llvm.cos.f32"[F32](this)
  fun sin(): F32 => @"llvm.sin.f32"[F32](this)
  fun tan(): F32 => @tanf[F32](this)

  fun cosh(): F32 => @coshf[F32](this)
  fun sinh(): F32 => @sinhf[F32](this)
  fun tanh(): F32 => @tanhf[F32](this)

  fun acos(): F32 => @acosf[F32](this)
  fun asin(): F32 => @asinf[F32](this)
  fun atan(): F32 => @atanf[F32](this)
  fun atan2(y: F32): F32 => @atan2f[F32](this, y)

  fun acosh(): F32 => @acoshf[F32](this)
  fun asinh(): F32 => @asinhf[F32](this)
  fun atanh(): F32 => @atanhf[F32](this)

  fun hash(): U64 => bits().hash()

  fun i128(): I128 => f64().i128()
  fun u128(): U128 => f64().u128()

primitive F64 is FloatingPoint[F64]
  new create(from: F64) => compiler_intrinsic
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U64) => compiler_intrinsic
  fun bits(): U64 => compiler_intrinsic

  fun abs(): F64 => @"llvm.fabs.f64"[F64](this)
  fun min(y: F64): F64 => if this < y then this else y end
    //@"llvm.minnum.f64"[F64](this, y)
  fun max(y: F64): F64 => if this > y then this else y end
    //@"llvm.maxnum.f64"[F64](this, y)

  fun ceil(): F64 => @"llvm.ceil.f64"[F64](this)
  fun floor(): F64 => @"llvm.floor.f64"[F64](this)
  fun round(): F64 => @"llvm.round.f64"[F64](this)
  fun trunc(): F64 => @"llvm.trunc.f64"[F64](this)

  fun finite(): Bool =>
    if Platform.windows() then
      @_finite[I32](this) != 0
    else
      @finite[I32](this) != 0
    end

  fun nan(): Bool =>
    if Platform.windows() then
      @_isnan[I32](this) != 0
    else
      @isnan[I32](this) != 0
    end

  fun frexp(): (F64, U32) =>
    var exponent: U32 = 0
    var mantissa = @frexp[F64](this, &exponent)
    (mantissa, exponent)

  fun log(): F64 => @"llvm.log.f64"[F64](this)
  fun log2(): F64 => @"llvm.log2.f64"[F64](this)
  fun log10(): F64 => @"llvm.log10.f64"[F64](this)
  fun logb(): F64 => @logb[F64](this)

  fun pow(y: F64): F64 => @"llvm.pow.f64"[F64](this, y)
  fun powi(y: I32): F64 =>
    if Platform.windows() then
      pow(y.f64())
    else
      @"llvm.powi.f64"[F64](this, y)
    end

  fun sqrt(): F64 => @"llvm.sqrt.f64"[F64](this)
  fun cbrt(): F64 => @cbrt[F64](this)
  fun exp(): F64 => @"llvm.exp.f64"[F64](this)
  fun exp2(): F64 => @"llvm.exp2.f64"[F64](this)

  fun cos(): F64 => @"llvm.cos.f64"[F64](this)
  fun sin(): F64 => @"llvm.sin.f64"[F64](this)
  fun tan(): F64 => @tan[F64](this)

  fun cosh(): F64 => @cosh[F64](this)
  fun sinh(): F64 => @sinh[F64](this)
  fun tanh(): F64 => @tanh[F64](this)

  fun acos(): F64 => @acos[F64](this)
  fun asin(): F64 => @asin[F64](this)
  fun atan(): F64 => @atan[F64](this)
  fun atan2(y: F64): F64 => @atan2[F64](this, y)

  fun acosh(): F64 => @acosh[F64](this)
  fun asinh(): F64 => @asinh[F64](this)
  fun atanh(): F64 => @atanh[F64](this)

  fun hash(): U64 => bits().hash()

  fun i128(): I128 =>
    let bit = bits()
    let high = (bit >> 32).u32()
    let ex = ((high and 0x7FF00000) >> 20) - 1023

    if ex < 0 then
      return 0
    end

    let s = ((high and 0x80000000) >> 31).i128()
    var r = ((bit and 0x000FFFFFFFFFFFFF) or 0x0010000000000000).i128()
    let ex' = ex.i128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (52 - ex')
    end

    (r xor s) - s

  fun u128(): U128 =>
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
