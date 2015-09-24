primitive I8 is _SignedInteger[I8, U8]
  new create(value: I8 = 0) => compiler_intrinsic
  fun tag from[A: (Number & Real[A] box)](a: A): I8 => a.i8()

  fun tag min_value(): I8 => -0x80
  fun tag max_value(): I8 => 0x7F

  fun abs(): U8 => if this < 0 then (-this).u8() else this.u8() end
  fun max(that: I8): I8 => if this > that then this else that end
  fun min(that: I8): I8 => if this < that then this else that end

  fun bswap(): I8 => this
  fun popcount(): I8 => @"llvm.ctpop.i8"[I8](this)
  fun clz(): I8 => @"llvm.ctlz.i8"[I8](this, false)
  fun ctz(): I8 => @"llvm.cttz.i8"[I8](this, false)
  fun bitwidth(): I8 => 8

  fun addc(y: I8): (I8, Bool) =>
    @"llvm.sadd.with.overflow.i8"[(I8, Bool)](this, y)
  fun subc(y: I8): (I8, Bool) =>
    @"llvm.ssub.with.overflow.i8"[(I8, Bool)](this, y)
  fun mulc(y: I8): (I8, Bool) =>
    @"llvm.smul.with.overflow.i8"[(I8, Bool)](this, y)

primitive I16 is _SignedInteger[I16, U16]
  new create(value: I16 = 0) => compiler_intrinsic
  fun tag from[A: (Number & Real[A] box)](a: A): I16 => a.i16()

  fun tag min_value(): I16 => -0x8000
  fun tag max_value(): I16 => 0x7FFF

  fun abs(): U16 => if this < 0 then (-this).u16() else this.u16() end
  fun max(that: I16): I16 => if this > that then this else that end
  fun min(that: I16): I16 => if this < that then this else that end

  fun bswap(): I16 => @"llvm.bswap.i16"[I16](this)
  fun popcount(): I16 => @"llvm.ctpop.i16"[I16](this)
  fun clz(): I16 => @"llvm.ctlz.i16"[I16](this, false)
  fun ctz(): I16 => @"llvm.cttz.i16"[I16](this, false)
  fun bitwidth(): I16 => 16

  fun addc(y: I16): (I16, Bool) =>
    @"llvm.sadd.with.overflow.i16"[(I16, Bool)](this, y)
  fun subc(y: I16): (I16, Bool) =>
    @"llvm.ssub.with.overflow.i16"[(I16, Bool)](this, y)
  fun mulc(y: I16): (I16, Bool) =>
    @"llvm.smul.with.overflow.i16"[(I16, Bool)](this, y)

primitive I32 is _SignedInteger[I32, U32]
  new create(value: I32 = 0) => compiler_intrinsic
  fun tag from[A: (Number & Real[A] box)](a: A): I32 => a.i32()

  fun tag min_value(): I32 => -0x8000_0000
  fun tag max_value(): I32 => 0x7FFF_FFFF

  fun abs(): U32 => if this < 0 then (-this).u32() else this.u32() end
  fun max(that: I32): I32 => if this > that then this else that end
  fun min(that: I32): I32 => if this < that then this else that end

  fun bswap(): I32 => @"llvm.bswap.i32"[I32](this)
  fun popcount(): I32 => @"llvm.ctpop.i32"[I32](this)
  fun clz(): I32 => @"llvm.ctlz.i32"[I32](this, false)
  fun ctz(): I32 => @"llvm.cttz.i32"[I32](this, false)
  fun bitwidth(): I32 => 32

  fun addc(y: I32): (I32, Bool) =>
    @"llvm.sadd.with.overflow.i32"[(I32, Bool)](this, y)
  fun subc(y: I32): (I32, Bool) =>
    @"llvm.ssub.with.overflow.i32"[(I32, Bool)](this, y)
  fun mulc(y: I32): (I32, Bool) =>
    @"llvm.smul.with.overflow.i32"[(I32, Bool)](this, y)

primitive I64 is _SignedInteger[I64, U64]
  new create(value: I64 = 0) => compiler_intrinsic
  fun tag from[A: (Number & Real[A] box)](a: A): I64 => a.i64()

  fun tag min_value(): I64 => -0x8000_0000_0000_0000
  fun tag max_value(): I64 => 0x7FFF_FFFF_FFFF_FFFF

  fun abs(): U64 => if this < 0 then (-this).u64() else this.u64() end
  fun max(that: I64): I64 => if this > that then this else that end
  fun min(that: I64): I64 => if this < that then this else that end

  fun bswap(): I64 => @"llvm.bswap.i64"[I64](this)
  fun popcount(): I64 => @"llvm.ctpop.i64"[I64](this)
  fun clz(): I64 => @"llvm.ctlz.i64"[I64](this, false)
  fun ctz(): I64 => @"llvm.cttz.i64"[I64](this, false)
  fun bitwidth(): I64 => 64

  fun addc(y: I64): (I64, Bool) =>
    @"llvm.sadd.with.overflow.i64"[(I64, Bool)](this, y)
  fun subc(y: I64): (I64, Bool) =>
    @"llvm.ssub.with.overflow.i64"[(I64, Bool)](this, y)
  fun mulc(y: I64): (I64, Bool) =>
    @"llvm.smul.with.overflow.i64"[(I64, Bool)](this, y)

primitive I128 is _SignedInteger[I128, U128]
  new create(value: I128 = 0) => compiler_intrinsic
  fun tag from[A: (Number & Real[A] box)](a: A): I128 => a.i128()

  fun tag min_value(): I128 => -0x8000_0000_0000_0000_0000_0000_0000_0000
  fun tag max_value(): I128 => 0x7FFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun abs(): U128 => if this < 0 then (-this).u128() else this.u128() end
  fun max(that: I128): I128 => if this > that then this else that end
  fun min(that: I128): I128 => if this < that then this else that end

  fun bswap(): I128 => @"llvm.bswap.i128"[I128](this)
  fun popcount(): I128 => @"llvm.ctpop.i128"[I128](this)
  fun clz(): I128 => @"llvm.ctlz.i128"[I128](this, false)
  fun ctz(): I128 => @"llvm.cttz.i128"[I128](this, false)
  fun bitwidth(): I128 => 128

  fun string(fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: U64 = 1, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    _ToString._u128(abs().u128(), this < 0, fmt, prefix, prec, width, align,
      fill)

  fun divmod(y: I128): (I128, I128) =>
    if Platform.has_i128() then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var num: I128 = this
      var den: I128 = y

      var minus = if num < 0 then
        num = -num
        true
      else
        false
      end

      if den < 0 then
        den = -den
        minus = not minus
      end

      (let q, let r) = num.u128().divmod(den.u128())
      (var q', var r') = (q.i128(), r.i128())

      if minus then
        q' = -q'
      end

      (q', r')
    end

  fun div(y: I128): I128 =>
    if Platform.has_i128() then
      this / y
    else
      (let q, let r) = divmod(y)
      q
    end

  fun mod(y: I128): I128 =>
    if Platform.has_i128() then
      this % y
    else
      (let q, let r) = divmod(y)
      r
    end

  fun f32(): F32 => this.f64().f32()

  fun f64(): F64 =>
    if this < 0 then
      -(-this).f64()
    else
      this.u128().f64()
    end
