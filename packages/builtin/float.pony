primitive F32 is Real[F32]
  new create(from: F64) => compiler_intrinsic
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U32) => compiler_intrinsic
  fun box bits(): U32 => compiler_intrinsic

  fun box min(y: F32): F32 => @llvm.minnum.f32[F32](this, y)
  fun box max(y: F32): F32 => @llvm.maxnum.f32[F32](this, y)

  fun box abs(): F32 => @llvm.fabs.f32[F32](this)
  fun box ceil(): F32 => @llvm.ceil.f32[F32](this)
  fun box floor(): F32 => @llvm.floor.f32[F32](this)
  fun box round(): F32 => @llvm.round.f32[F32](this)
  fun box trunc(): F32 => @llvm.trunc.f32[F32](this)

  fun box finite(): Bool =>
  I32(0) != if Platform.windows() then
      @_finite[I32](this.f64())
    elseif Platform.osx() then
      @finite[I32](this.f64())
    else
      @finitef[I32](this)
    end

  fun box nan(): Bool =>
    I32(0) != if Platform.windows() then
      @_isnan[I32](this.f64())
    elseif Platform.osx() then
      @isnan[I32](this.f64())
    else
      @isnanf[I32](this)
    end

  fun box log(): F32 => @llvm.log.f32[F32](this)
  fun box log2(): F32 => @llvm.log2.f32[F32](this)
  fun box log10(): F32 => @llvm.log10.f32[F32](this)
  fun box logb(): F32 => @logbf[F32](this)

  fun box pow(y: F32): F32 => @llvm.pow.f32[F32](this, y)
  fun box powi(y: I32): F32 => @llvm.powi.f32[F32](this, y)
  fun box sqrt(): F32 => @llvm.sqrt.f32[F32](this)
  fun box cbrt(): F32 => @cbrtf[F32](this)
  fun box exp(): F32 => @llvm.exp.f32[F32](this)
  fun box exp2(): F32 => @llvm.exp2.f32[F32](this)

  fun box cos(): F32 => @llvm.cos.f32[F32](this)
  fun box sin(): F32 => @llvm.sin.f32[F32](this)
  fun box tan(): F32 => @tanf[F32](this)

  fun box cosh(): F32 => @coshf[F32](this)
  fun box sinh(): F32 => @sinhf[F32](this)
  fun box tanh(): F32 => @tanhf[F32](this)

  fun box acos(): F32 => @acosf[F32](this)
  fun box asin(): F32 => @asinf[F32](this)
  fun box atan(): F32 => @atanf[F32](this)
  fun box atan2(y: F32): F32 => @atan2f[F32](this, y)

  fun box acosh(): F32 => @acoshf[F32](this)
  fun box asinh(): F32 => @asinhf[F32](this)
  fun box atanh(): F32 => @atanhf[F32](this)

  fun box string(): String iso^ => recover String.from_f32(this) end
  fun box hash(): U64 => bits().hash()

  fun box i128(): I128 => f64().i128()
  fun box u128(): U128 => f64().u128()

primitive F64 is Real[F64]
  new create(from: F64) => compiler_intrinsic
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U64) => compiler_intrinsic
  fun box bits(): U64 => compiler_intrinsic

  fun box min(y: F64): F64 => @llvm.minnum.f64[F64](this, y)
  fun box max(y: F64): F64 => @llvm.maxnum.f64[F64](this, y)

  fun box abs(): F64 => @llvm.fabs.f64[F64](this)
  fun box ceil(): F64 => @llvm.ceil.f64[F64](this)
  fun box floor(): F64 => @llvm.floor.f64[F64](this)
  fun box round(): F64 => @llvm.round.f64[F64](this)
  fun box trunc(): F64 => @llvm.trunc.f64[F64](this)

  fun box finite(): Bool =>
    I32(0) != if Platform.windows() then
      @_finite[I32](this)
    else
      @finite[I32](this)
    end

  fun box nan(): Bool =>
    I32(0) != if Platform.windows() then
      @_isnan[I32](this)
    else
      @isnan[I32](this)
    end

  fun box log(): F64 => @llvm.log.f64[F64](this)
  fun box log2(): F64 => @llvm.log2.f64[F64](this)
  fun box log10(): F64 => @llvm.log10.f64[F64](this)
  fun box logb(): F64 => @logb[F64](this)

  fun box pow(y: F64): F64 => @llvm.pow.f64[F64](this, y)
  fun box powi(y: I32): F64 => @llvm.powi.f64[F64](this, y)
  fun box sqrt(): F64 => @llvm.sqrt.f64[F64](this)
  fun box cbrt(): F64 => @cbrt[F64](this)
  fun box exp(): F64 => @llvm.exp.f64[F64](this)
  fun box exp2(): F64 => @llvm.exp2.f64[F64](this)

  fun box cos(): F64 => @llvm.cos.f64[F64](this)
  fun box sin(): F64 => @llvm.sin.f64[F64](this)
  fun box tan(): F64 => @tan[F64](this)

  fun box cosh(): F64 => @cosh[F64](this)
  fun box sinh(): F64 => @sinh[F64](this)
  fun box tanh(): F64 => @tanh[F64](this)

  fun box acos(): F64 => @acos[F64](this)
  fun box asin(): F64 => @asin[F64](this)
  fun box atan(): F64 => @atan[F64](this)
  fun box atan2(y: F64): F64 => @atan2[F64](this, y)

  fun box acosh(): F64 => @acosh[F64](this)
  fun box asinh(): F64 => @asinh[F64](this)
  fun box atanh(): F64 => @atanh[F64](this)

  fun box string(): String iso^ => recover String.from_f64(this) end
  fun box hash(): U64 => bits().hash()

  fun box i128(): I128 =>
    var bit = bits()
    var high = (bit >> 32).u32()
    var ex = ((high and 0x7FF00000) >> 20) - 1023

    if ex < 0 then
      return I128(0)
    end

    var s = ((high and 0x80000000) >> 31).i128()
    var r = ((bit and 0x000FFFFFFFFFFFFF) or 0x0010000000000000).i128()
    var ex' = ex.i128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (I128(52) - ex')
    end

    (r xor s) - s

  fun box u128(): U128 =>
    var bit = bits()
    var high = (bit >> 32).u32()
    var ex = ((high and 0x7FF00000) >> 20) - 1023

    if (ex < 0) or ((high and 0x80000000) != 0) then
      return U128(0)
    end

    var r = ((bit and 0x000FFFFFFFFFFFFF) or 0x0010000000000000).u128()
    var ex' = ex.u128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (U128(52) - ex')
    end

    r.u128()
