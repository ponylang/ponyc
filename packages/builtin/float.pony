primitive F32 is Stringable, ArithmeticConvertible
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U32) => compiler_intrinsic
  fun tag bits(): U32 => compiler_intrinsic

  fun tag abs(): F32 => if this < 0 then -this else this end
  fun tag ceil(): F32 => @ceilf[F32](this)
  fun tag floor(): F32 => @floorf[F32](this)
  fun tag round(): F32 => @roundf[F32](this)
  fun tag trunc(): F32 => @truncf[F32](this)

  fun tag finite(): Bool =>
    0 != if Platform.windows() then
      @_finite[I32](this.f64())
    elseif Platform.osx() then
      @finite[I32](this.f64())
    else
      @finitef[I32](this)
    end

  fun tag nan(): Bool =>
    0 != if Platform.windows() then
      @_isnan[I32](this.f64())
    elseif Platform.osx() then
      @isnan[I32](this.f64())
    else
      @isnanf[I32](this)
    end

  fun tag log(): F32 => @logf[F32](this)
  fun tag log10(): F32 => @log10f[F32](this)
  fun tag logb(): F32 => @logbf[F32](this)

  fun tag pow(y: F32): F32 => @powf[F32](this, y)
  fun tag sqrt(): F32 => @sqrtf[F32](this)
  fun tag cbrt(): F32 => @cbrtf[F32](this)
  fun tag exp(): F32 => @expf[F32](this)

  fun tag cos(): F32 => @cosf[F32](this)
  fun tag sin(): F32 => @sinf[F32](this)
  fun tag tan(): F32 => @tanf[F32](this)

  fun tag cosh(): F32 => @coshf[F32](this)
  fun tag sinh(): F32 => @sinhf[F32](this)
  fun tag tanh(): F32 => @tanhf[F32](this)

  fun tag acos(): F32 => @acosf[F32](this)
  fun tag asin(): F32 => @asinf[F32](this)
  fun tag atan(): F32 => @atanf[F32](this)
  fun tag atan2(y: F32): F32 => @atan2f[F32](this, y)

  fun tag acosh(): F32 => @acoshf[F32](this)
  fun tag asinh(): F32 => @asinhf[F32](this)
  fun tag atanh(): F32 => @atanhf[F32](this)

  fun tag string(): String iso^ => recover String.from_f32(this) end

  fun tag i128(): I128 => f64().i128()
  fun tag u128(): U128 => f64().u128()

primitive F64 is Stringable, ArithmeticConvertible
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U64) => compiler_intrinsic
  fun tag bits(): U64 => compiler_intrinsic

  fun tag abs(): F64 => if this < 0 then -this else this end
  fun tag ceil(): F64 => @ceil[F64](this)
  fun tag floor(): F64 => @floor[F64](this)
  fun tag round(): F64 => @round[F64](this)
  fun tag trunc(): F64 => @trunc[F64](this)

  fun tag finite(): Bool =>
    0 != if Platform.windows() then
      @_finite[I32](this)
    else
      @finite[I32](this)
    end

  fun tag nan(): Bool =>
    0 != if Platform.windows() then
      @_isnan[I32](this)
    else
      @isnan[I32](this)
    end

  fun tag log(): F64 => @log[F64](this)
  fun tag log10(): F64 => @log10[F64](this)
  fun tag logb(): F64 => @logb[F64](this)

  fun tag pow(y: F64): F64 => @pow[F64](this, y)
  fun tag sqrt(): F64 => @sqrt[F64](this)
  fun tag cbrt(): F64 => @cbrt[F64](this)
  fun tag exp(): F64 => @exp[F64](this)

  fun tag cos(): F64 => @cos[F64](this)
  fun tag sin(): F64 => @sin[F64](this)
  fun tag tan(): F64 => @tan[F64](this)

  fun tag cosh(): F64 => @cosh[F64](this)
  fun tag sinh(): F64 => @sinh[F64](this)
  fun tag tanh(): F64 => @tanh[F64](this)

  fun tag acos(): F64 => @acos[F64](this)
  fun tag asin(): F64 => @asin[F64](this)
  fun tag atan(): F64 => @atan[F64](this)
  fun tag atan2(y: F64): F64 => @atan2[F64](this, y)

  fun tag acosh(): F64 => @acosh[F64](this)
  fun tag asinh(): F64 => @asinh[F64](this)
  fun tag atanh(): F64 => @atanh[F64](this)

  fun tag string(): String iso^ => recover String.from_f64(this) end

  fun tag i128(): I128 =>
    var bit = bits()
    var high = (bit >> 32).u32()
    var ex = ((high and 0x7FF00000) >> 20) - 1023

    if ex < 0 then
      return 0.i128()
    end

    var s = ((high and 0x80000000) >> 31).i128()
    var r = (0x0010000000000000 or (0x000FFFFFFFFFFFFF and bit)).i128()
    var ex' = ex.i128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (52 - ex')
    end

    (r xor s) - s

  fun tag u128(): U128 =>
    var bit = bits()
    var high = (bit >> 32).u32()
    var ex = ((high and 0x7FF00000) >> 20) - 1023

    if (ex < 0) or ((high and 0x80000000) != 0) then
      return 0.u128()
    end

    var r = (0x0010000000000000 or (0x000FFFFFFFFFFFFF and bit)).u128()
    var ex' = ex.u128()

    if ex' > 52 then
      r = r << (ex' - 52)
    else
      r = r >> (52 - ex')
    end

    r.u128()
