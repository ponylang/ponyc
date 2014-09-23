primitive F32 is Stringable, ArithmeticConvertible
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U32) => compiler_intrinsic
  fun val bits(): U32 => compiler_intrinsic

  fun val abs(): F32 => if this < 0 then -this else this end
  fun val ceil(): F32 => @ceilf[F32](this)
  fun val floor(): F32 => @floorf[F32](this)
  fun val round(): F32 => @roundf[F32](this)
  fun val trunc(): F32 => @truncf[F32](this)

  fun val finite(): Bool =>
    0 != if Platform.windows() then
      @_finite[I32](this.f64())
    elseif Platform.osx() then
      @finite[I32](this.f64())
    else
      @finitef[I32](this)
    end

  fun val nan(): Bool =>
    0 != if Platform.windows() then
      @_isnan[I32](this.f64())
    elseif Platform.osx() then
      @isnan[I32](this.f64())
    else
      @isnanf[I32](this)
    end

  fun val log(): F32 => @logf[F32](this)
  fun val log10(): F32 => @log10f[F32](this)
  fun val logb(): F32 => @logbf[F32](this)

  fun val pow(y: F32): F32 => @powf[F32](this, y)
  fun val sqrt(): F32 => @sqrtf[F32](this)
  fun val cbrt(): F32 => @cbrtf[F32](this)
  fun val exp(): F32 => @expf[F32](this)

  fun val cos(): F32 => @cosf[F32](this)
  fun val sin(): F32 => @sinf[F32](this)
  fun val tan(): F32 => @tanf[F32](this)

  fun val cosh(): F32 => @coshf[F32](this)
  fun val sinh(): F32 => @sinhf[F32](this)
  fun val tanh(): F32 => @tanhf[F32](this)

  fun val acos(): F32 => @acosf[F32](this)
  fun val asin(): F32 => @asinf[F32](this)
  fun val atan(): F32 => @atanf[F32](this)
  fun val atan2(y: F32): F32 => @atan2f[F32](this, y)

  fun val acosh(): F32 => @acoshf[F32](this)
  fun val asinh(): F32 => @asinhf[F32](this)
  fun val atanh(): F32 => @atanhf[F32](this)

  fun box string(): String => "F32"

primitive F64 is Stringable, ArithmeticConvertible
  new pi() => compiler_intrinsic
  new e() => compiler_intrinsic

  new from_bits(i: U64) => compiler_intrinsic
  fun val bits(): U64 => compiler_intrinsic

  fun val abs(): F64 => if this < 0 then -this else this end
  fun val ceil(): F64 => @ceil[F64](this)
  fun val floor(): F64 => @floor[F64](this)
  fun val round(): F64 => @round[F64](this)
  fun val trunc(): F64 => @trunc[F64](this)

  fun val finite(): Bool =>
    0 != if Platform.windows() then
      @_finite[I32](this)
    else
      @finite[I32](this)
    end

  fun val nan(): Bool =>
    0 != if Platform.windows() then
      @_isnan[I32](this)
    else
      @isnan[I32](this)
    end

  fun val log(): F64 => @log[F64](this)
  fun val log10(): F64 => @log10[F64](this)
  fun val logb(): F64 => @logb[F64](this)

  fun val pow(y: F64): F64 => @pow[F64](this, y)
  fun val sqrt(): F64 => @sqrt[F64](this)
  fun val cbrt(): F64 => @cbrt[F64](this)
  fun val exp(): F64 => @exp[F64](this)

  fun val cos(): F64 => @cos[F64](this)
  fun val sin(): F64 => @sin[F64](this)
  fun val tan(): F64 => @tab[F64](this)

  fun val cosh(): F64 => @cosh[F64](this)
  fun val sinh(): F64 => @sinh[F64](this)
  fun val tanh(): F64 => @tabh[F64](this)

  fun val acos(): F64 => @acos[F64](this)
  fun val asin(): F64 => @asin[F64](this)
  fun val atan(): F64 => @atab[F64](this)
  fun val atan2(y: F64): F64 => @atan2[F64](this, y)

  fun val acosh(): F64 => @acosh[F64](this)
  fun val asinh(): F64 => @asinh[F64](this)
  fun val atanh(): F64 => @atabh[F64](this)

  fun box string(): String => "F64"
