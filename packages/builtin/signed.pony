primitive I8 is _SignedInteger[I8, U8]
  new create(value: I8) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i8()

  new min_value() => -0x80
  new max_value() => 0x7F

  fun abs(): U8 => if this < 0 then (-this).u8() else this.u8() end
  fun bswap(): I8 => this
  fun popcount(): U8 => @"llvm.ctpop.i8"[U8](this)
  fun clz(): U8 => @"llvm.ctlz.i8"[U8](this, false)
  fun ctz(): U8 => @"llvm.cttz.i8"[U8](this, false)

  fun clz_unsafe(): U8 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i8"[U8](this, true)

  fun ctz_unsafe(): U8 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i8"[U8](this, true)

  fun bitwidth(): U8 => 8

  fun min(y: I8): I8 => if this < y then this else y end
  fun max(y: I8): I8 => if this > y then this else y end

  fun addc(y: I8): (I8, Bool) =>
    @"llvm.sadd.with.overflow.i8"[(I8, Bool)](this, y)

  fun subc(y: I8): (I8, Bool) =>
    @"llvm.ssub.with.overflow.i8"[(I8, Bool)](this, y)

  fun mulc(y: I8): (I8, Bool) =>
    @"llvm.smul.with.overflow.i8"[(I8, Bool)](this, y)

primitive I16 is _SignedInteger[I16, U16]
  new create(value: I16) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i16()

  new min_value() => -0x8000
  new max_value() => 0x7FFF

  fun abs(): U16 => if this < 0 then (-this).u16() else this.u16() end
  fun bswap(): I16 => @"llvm.bswap.i16"[I16](this)
  fun popcount(): U16 => @"llvm.ctpop.i16"[U16](this)
  fun clz(): U16 => @"llvm.ctlz.i16"[U16](this, false)
  fun ctz(): U16 => @"llvm.cttz.i16"[U16](this, false)

  fun clz_unsafe(): U16 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i16"[U16](this, true)

  fun ctz_unsafe(): U16 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i16"[U16](this, true)

  fun bitwidth(): U16 => 16

  fun min(y: I16): I16 => if this < y then this else y end
  fun max(y: I16): I16 => if this > y then this else y end

  fun addc(y: I16): (I16, Bool) =>
    @"llvm.sadd.with.overflow.i16"[(I16, Bool)](this, y)

  fun subc(y: I16): (I16, Bool) =>
    @"llvm.ssub.with.overflow.i16"[(I16, Bool)](this, y)

  fun mulc(y: I16): (I16, Bool) =>
    @"llvm.smul.with.overflow.i16"[(I16, Bool)](this, y)

primitive I32 is _SignedInteger[I32, U32]
  new create(value: I32) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i32()

  new min_value() => -0x8000_0000
  new max_value() => 0x7FFF_FFFF

  fun abs(): U32 => if this < 0 then (-this).u32() else this.u32() end
  fun bswap(): I32 => @"llvm.bswap.i32"[I32](this)
  fun popcount(): U32 => @"llvm.ctpop.i32"[U32](this)
  fun clz(): U32 => @"llvm.ctlz.i32"[U32](this, false)
  fun ctz(): U32 => @"llvm.cttz.i32"[U32](this, false)

  fun clz_unsafe(): U32 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i32"[U32](this, true)

  fun ctz_unsafe(): U32 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i32"[U32](this, true)

  fun bitwidth(): U32 => 32

  fun min(y: I32): I32 => if this < y then this else y end
  fun max(y: I32): I32 => if this > y then this else y end

  fun addc(y: I32): (I32, Bool) =>
    @"llvm.sadd.with.overflow.i32"[(I32, Bool)](this, y)

  fun subc(y: I32): (I32, Bool) =>
    @"llvm.ssub.with.overflow.i32"[(I32, Bool)](this, y)

  fun mulc(y: I32): (I32, Bool) =>
    @"llvm.smul.with.overflow.i32"[(I32, Bool)](this, y)

primitive I64 is _SignedInteger[I64, U64]
  new create(value: I64) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i64()

  new min_value() => -0x8000_0000_0000_0000
  new max_value() => 0x7FFF_FFFF_FFFF_FFFF

  fun abs(): U64 => if this < 0 then (-this).u64() else this.u64() end
  fun bswap(): I64 => @"llvm.bswap.i64"[I64](this)
  fun popcount(): U64 => @"llvm.ctpop.i64"[U64](this)
  fun clz(): U64 => @"llvm.ctlz.i64"[U64](this, false)
  fun ctz(): U64 => @"llvm.cttz.i64"[U64](this, false)

  fun clz_unsafe(): U64 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i64"[U64](this, true)

  fun ctz_unsafe(): U64 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i64"[U64](this, true)

  fun bitwidth(): U64 => 64

  fun min(y: I64): I64 => if this < y then this else y end
  fun max(y: I64): I64 => if this > y then this else y end
  fun hash(): USize => u64().hash()

  fun addc(y: I64): (I64, Bool) =>
    @"llvm.sadd.with.overflow.i64"[(I64, Bool)](this, y)

  fun subc(y: I64): (I64, Bool) =>
    @"llvm.ssub.with.overflow.i64"[(I64, Bool)](this, y)

  fun mulc(y: I64): (I64, Bool) =>
    _SignedCheckedArithmetic._mulc[U64, I64](this, y)


primitive ILong is _SignedInteger[ILong, ULong]
  new create(value: ILong) => value
  new from[A: (Number & Real[A] val)](a: A) => a.ilong()

  new min_value() =>
    ifdef ilp32 or llp64 then
      -0x8000_0000
    else
      -0x8000_0000_0000_0000
    end

  new max_value() =>
    ifdef ilp32 or llp64 then
      0x7FFF_FFFF
    else
      0x7FFF_FFFF_FFFF_FFFF
    end

  fun abs(): ULong => if this < 0 then (-this).ulong() else this.ulong() end

  fun bswap(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.bswap.i32"[ILong](this)
    else
      @"llvm.bswap.i64"[ILong](this)
    end

  fun popcount(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctpop.i32"[ULong](this)
    else
      @"llvm.ctpop.i64"[ULong](this)
    end

  fun clz(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"[ULong](this, false)
    else
      @"llvm.ctlz.i64"[ULong](this, false)
    end

  fun ctz(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"[ULong](this, false)
    else
      @"llvm.cttz.i64"[ULong](this, false)
    end

  fun clz_unsafe(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"[ULong](this, true)
    else
      @"llvm.ctlz.i64"[ULong](this, true)
    end

  fun ctz_unsafe(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"[ULong](this, false)
    else
      @"llvm.cttz.i64"[ULong](this, true)
    end

  fun bitwidth(): ULong => ifdef ilp32 or llp64 then 32 else 64 end
  fun min(y: ILong): ILong => if this < y then this else y end
  fun max(y: ILong): ILong => if this > y then this else y end
  fun hash(): USize => ulong().hash()

  fun addc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.sadd.with.overflow.i32"[(ILong, Bool)](this, y)
    else
      @"llvm.sadd.with.overflow.i64"[(ILong, Bool)](this, y)
    end

  fun subc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.ssub.with.overflow.i32"[(ILong, Bool)](this, y)
    else
      @"llvm.ssub.with.overflow.i64"[(ILong, Bool)](this, y)
    end

  fun mulc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.smul.with.overflow.i32"[(ILong, Bool)](this, y)
    else
      _SignedCheckedArithmetic._mulc[ULong, ILong](this, y)
    end

primitive ISize is _SignedInteger[ISize, USize]
  new create(value: ISize) => value
  new from[A: (Number & Real[A] val)](a: A) => a.isize()

  new min_value() =>
    ifdef ilp32 then
      -0x8000_0000
    else
      -0x8000_0000_0000_0000
    end

  new max_value() =>
    ifdef ilp32 then
      0x7FFF_FFFF
    else
      0x7FFF_FFFF_FFFF_FFFF
    end

  fun abs(): USize => if this < 0 then (-this).usize() else this.usize() end

  fun bswap(): ISize =>
    ifdef ilp32 then
      @"llvm.bswap.i32"[ISize](this)
    else
      @"llvm.bswap.i64"[ISize](this)
    end

  fun popcount(): USize =>
    ifdef ilp32 then
      @"llvm.ctpop.i32"[USize](this)
    else
      @"llvm.ctpop.i64"[USize](this)
    end

  fun clz(): USize =>
    ifdef ilp32 then
      @"llvm.ctlz.i32"[USize](this, false)
    else
      @"llvm.ctlz.i64"[USize](this, false)
    end

  fun ctz(): USize =>
    ifdef ilp32 then
      @"llvm.cttz.i32"[USize](this, false)
    else
      @"llvm.cttz.i64"[USize](this, false)
    end

  fun clz_unsafe(): USize =>
    ifdef ilp32 then
      @"llvm.ctlz.i32"[USize](this, true)
    else
      @"llvm.ctlz.i64"[USize](this, true)
    end

  fun ctz_unsafe(): USize =>
    ifdef ilp32 then
      @"llvm.cttz.i32"[USize](this, true)
    else
      @"llvm.cttz.i64"[USize](this, true)
    end

  fun bitwidth(): USize => ifdef ilp32 then 32 else 64 end
  fun min(y: ISize): ISize => if this < y then this else y end
  fun max(y: ISize): ISize => if this > y then this else y end

  fun addc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      @"llvm.sadd.with.overflow.i32"[(ISize, Bool)](this, y)
    else
      @"llvm.sadd.with.overflow.i64"[(ISize, Bool)](this, y)
    end

  fun subc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      @"llvm.ssub.with.overflow.i32"[(ISize, Bool)](this, y)
    else
      @"llvm.ssub.with.overflow.i64"[(ISize, Bool)](this, y)
    end

  fun mulc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      @"llvm.smul.with.overflow.i32"[(ISize, Bool)](this, y)
    else
      _SignedCheckedArithmetic._mulc[USize, ISize](this, y)
    end

primitive I128 is _SignedInteger[I128, U128]
  new create(value: I128) => value
  new from[A: (Number & Real[A] val)](a: A) => a.i128()

  new min_value() => -0x8000_0000_0000_0000_0000_0000_0000_0000
  new max_value() => 0x7FFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun abs(): U128 => if this < 0 then (-this).u128() else this.u128() end
  fun bswap(): I128 => @"llvm.bswap.i128"[I128](this)
  fun popcount(): U128 => @"llvm.ctpop.i128"[U128](this)
  fun clz(): U128 => @"llvm.ctlz.i128"[U128](this, false)
  fun ctz(): U128 => @"llvm.cttz.i128"[U128](this, false)

  fun clz_unsafe(): U128 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.ctlz.i128"[U128](this, true)

  fun ctz_unsafe(): U128 =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    @"llvm.cttz.i128"[U128](this, true)

  fun bitwidth(): U128 => 128
  fun min(y: I128): I128 => if this < y then this else y end
  fun max(y: I128): I128 => if this > y then this else y end
  fun hash(): USize => u128().hash()
  fun hash64(): U64 => u128().hash64()

  fun string(): String iso^ =>
    _ToString._u128(abs().u128(), this < 0)

  fun mul(y: I128): I128 =>
    (u128() * y.u128()).i128()

  fun divmod(y: I128): (I128, I128) =>
    ifdef native128 then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var num: I128 = if this >= 0 then this else -this end
      var den: I128 = if y >= 0 then y else -y end

      (let q, let r) = num.u128().divmod(den.u128())
      (var q', var r') = (q.i128(), r.i128())

      if this < 0 then
        r' = -r'

        if y > 0 then
          q' = -q'
        end
      elseif y < 0 then
        q' = -q'
      end

      (q', r')
    end

  fun div(y: I128): I128 =>
    ifdef native128 then
      this / y
    else
      (let q, let r) = divmod(y)
      q
    end

  fun mod(y: I128): I128 =>
    ifdef native128 then
      this % y
    else
      (let q, let r) = divmod(y)
      r
    end

  fun mul_unsafe(y: I128): I128 =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this *~ y
    else
      this * y
    end

  fun divmod_unsafe(y: I128): (I128, I128) =>
    """
    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      (this *~ y, this /~ y)
    else
      divmod(y)
    end

  fun div_unsafe(y: I128): I128 =>
    """
    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this /~ y
    else
      this / y
    end

  fun mod_unsafe(y: I128): I128 =>
    """
    Unsafe operation.
    If y is 0, the result is undefined.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this %~ y
    else
      this % y
    end

  fun f32(): F32 =>
    f64().f32()

  fun f64(): F64 =>
    if this < 0 then
      -(-u128()).f64()
    else
      u128().f64()
    end

  fun f32_unsafe(): F32 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    f64_unsafe().f32_unsafe()

  fun f64_unsafe(): F64 =>
    """
    Unsafe operation.
    If the value doesn't fit in the destination type, the result is undefined.
    """
    f64()

  fun addc(y: I128): (I128, Bool) =>
    ifdef native128 then
      @"llvm.sadd.with.overflow.i128"[(I128, Bool)](this, y)
    else
      let overflow =
        if y > 0 then
          (this > (max_value() - y))
        else
          (this < (min_value() - y))
        end

      (this + y, overflow)
    end

  fun subc(y: I128): (I128, Bool) =>
    ifdef native128 then
      @"llvm.ssub.with.overflow.i128"[(I128, Bool)](this, y)
    else
      let overflow =
        if y > 0 then
          (this < (min_value() + y))
        else
          (this > (max_value() + y))
        end

      (this - y, overflow)
    end

  fun mulc(y: I128): (I128, Bool) =>
    // using llvm.smul.with.overflow.i128 would require to link
    // llvm compiler-rt where the function implementing it lives: https://github.com/llvm-mirror/compiler-rt/blob/master/lib/builtins/muloti4.c
    // See this bug for reference:
    // the following implementation is more or less exactly was __muloti4 is
    // doing
    _SignedCheckedArithmetic._mulc[U128, I128](this, y)

type Signed is (I8 | I16 | I32 | I64 | I128 | ILong | ISize)


primitive _SignedCheckedArithmetic
  fun _mulc[U: _UnsignedInteger[U] val, T: (Signed & _SignedInteger[T, U] val)](x: T, y: T): (T, Bool) =>
    """
    basically exactly what the runtime functions __muloti4, mulodi4 etc. are doing
    and roughly as fast as these.

    Additionally on (at least some) 32 bit systems, the runtime function for checked 64 bit integer addition __mulodi4 is not available.
    So we shouldn't use: `@"llvm.smul.with.overflow.i64"[(I64, Bool)](this, y)`

    Also see https://bugs.llvm.org/show_bug.cgi?id=14469

    That's basically why we rolled our own.
    """
    let result = x * y
    if x == T.min_value() then
      return (result, (y != T.from[I8](0)) and (y != T.from[I8](1)))
    end
    if y == T.min_value() then
      return (result, (x != T.from[I8](0)) and (x != T.from[I8](1)))
    end
    let x_neg = x >> (x.bitwidth() - U.from[U8](1))
    let x_abs = (x xor x_neg) - x_neg
    let y_neg = y >> (x.bitwidth() - U.from[U8](1))
    let y_abs = (y xor y_neg) - y_neg

    if ((x_abs < T.from[I8](2)) or (y_abs < T.from[I8](2))) then
      return (result, false)
    end
    if (x_neg == y_neg) then
      (result, (x_abs > (T.max_value() / y_abs)))
    else
      (result, (x_abs > (T.min_value() / -y_abs)))
    end



