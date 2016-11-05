primitive U8 is _UnsignedInteger[U8]
  new create(value: U8 = 0) => value
  new from[B: (Number & Real[B] val)](a: B) => a.u8()

  new min_value() => 0
  new max_value() => 0xFF

  fun next_pow2(): U8 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U8 => this
  fun bswap(): U8 => this
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
  fun min(y: U8): U8 => if this < y then this else y end
  fun max(y: U8): U8 => if this > y then this else y end

  fun addc(y: U8): (U8, Bool) =>
    @"llvm.uadd.with.overflow.i8"[(U8, Bool)](this, y)
  fun subc(y: U8): (U8, Bool) =>
    @"llvm.usub.with.overflow.i8"[(U8, Bool)](this, y)
  fun mulc(y: U8): (U8, Bool) =>
    @"llvm.umul.with.overflow.i8"[(U8, Bool)](this, y)

primitive U16 is _UnsignedInteger[U16]
  new create(value: U16 = 0) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u16()

  new min_value() => 0
  new max_value() => 0xFFFF

  fun next_pow2(): U16 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U16 => this
  fun bswap(): U16 => @"llvm.bswap.i16"[U16](this)
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
  fun min(y: U16): U16 => if this < y then this else y end
  fun max(y: U16): U16 => if this > y then this else y end

  fun addc(y: U16): (U16, Bool) =>
    @"llvm.uadd.with.overflow.i16"[(U16, Bool)](this, y)
  fun subc(y: U16): (U16, Bool) =>
    @"llvm.usub.with.overflow.i16"[(U16, Bool)](this, y)
  fun mulc(y: U16): (U16, Bool) =>
    @"llvm.umul.with.overflow.i16"[(U16, Bool)](this, y)

primitive U32 is _UnsignedInteger[U32]
  new create(value: U32 = 0) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u32()

  new min_value() => 0
  new max_value() => 0xFFFF_FFFF

  fun next_pow2(): U32 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U32 => this
  fun bswap(): U32 => @"llvm.bswap.i32"[U32](this)
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
  fun min(y: U32): U32 => if this < y then this else y end
  fun max(y: U32): U32 => if this > y then this else y end

  fun addc(y: U32): (U32, Bool) =>
    @"llvm.uadd.with.overflow.i32"[(U32, Bool)](this, y)
  fun subc(y: U32): (U32, Bool) =>
    @"llvm.usub.with.overflow.i32"[(U32, Bool)](this, y)
  fun mulc(y: U32): (U32, Bool) =>
    @"llvm.umul.with.overflow.i32"[(U32, Bool)](this, y)

primitive U64 is _UnsignedInteger[U64]
  new create(value: U64 = 0) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u64()

  new min_value() => 0
  new max_value() => 0xFFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U64 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U64 => this
  fun bswap(): U64 => @"llvm.bswap.i64"[U64](this)
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
  fun min(y: U64): U64 => if this < y then this else y end
  fun max(y: U64): U64 => if this > y then this else y end

  fun addc(y: U64): (U64, Bool) =>
    @"llvm.uadd.with.overflow.i64"[(U64, Bool)](this, y)
  fun subc(y: U64): (U64, Bool) =>
    @"llvm.usub.with.overflow.i64"[(U64, Bool)](this, y)
  fun mulc(y: U64): (U64, Bool) =>
    @"llvm.umul.with.overflow.i64"[(U64, Bool)](this, y)

primitive ULong is _UnsignedInteger[ULong]
  new create(value: ULong = 0) => value
  new from[A: (Number & Real[A] val)](a: A) => a.ulong()

  new min_value() => 0

  new max_value() =>
    ifdef ilp32 or llp64 then
      0xFFFF_FFFF
    else
      0xFFFF_FFFF_FFFF_FFFF
    end

  fun next_pow2(): ULong =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): ULong => this

  fun bswap(): ULong =>
    ifdef ilp32 or llp64 then
      @"llvm.bswap.i32"[ULong](this)
    else
      @"llvm.bswap.i64"[ULong](this)
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
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"[ULong](this, true)
    else
      @"llvm.ctlz.i64"[ULong](this, true)
    end

  fun ctz_unsafe(): ULong =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"[ULong](this, false)
    else
      @"llvm.cttz.i64"[ULong](this, true)
    end

  fun bitwidth(): ULong => ifdef ilp32 or llp64 then 32 else 64 end
  fun min(y: ULong): ULong => if this < y then this else y end
  fun max(y: ULong): ULong => if this > y then this else y end

  fun addc(y: ULong): (ULong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.uadd.with.overflow.i32"[(ULong, Bool)](this, y)
    else
      @"llvm.uadd.with.overflow.i64"[(ULong, Bool)](this, y)
    end

  fun subc(y: ULong): (ULong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.usub.with.overflow.i32"[(ULong, Bool)](this, y)
    else
      @"llvm.usub.with.overflow.i64"[(ULong, Bool)](this, y)
    end

  fun mulc(y: ULong): (ULong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.umul.with.overflow.i32"[(ULong, Bool)](this, y)
    else
      @"llvm.umul.with.overflow.i64"[(ULong, Bool)](this, y)
    end

primitive USize is _UnsignedInteger[USize]
  new create(value: USize = 0) => value
  new from[A: (Number & Real[A] val)](a: A) => a.usize()

  new min_value() => 0

  new max_value() =>
    ifdef ilp32 then
      0xFFFF_FFFF
    else
      0xFFFF_FFFF_FFFF_FFFF
    end

  fun next_pow2(): USize =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): USize => this

  fun bswap(): USize =>
    ifdef ilp32 then
      @"llvm.bswap.i32"[USize](this)
    else
      @"llvm.bswap.i64"[USize](this)
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
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 then
      @"llvm.ctlz.i32"[USize](this, true)
    else
      @"llvm.ctlz.i64"[USize](this, true)
    end

  fun ctz_unsafe(): USize =>
    """
    Unsafe operation.
    If this is 0, the result is undefined.
    """
    ifdef ilp32 then
      @"llvm.cttz.i32"[USize](this, true)
    else
      @"llvm.cttz.i64"[USize](this, true)
    end

  fun bitwidth(): USize => ifdef ilp32 then 32 else 64 end
  fun min(y: USize): USize => if this < y then this else y end
  fun max(y: USize): USize => if this > y then this else y end

  fun addc(y: USize): (USize, Bool) =>
    ifdef ilp32 then
      @"llvm.uadd.with.overflow.i32"[(USize, Bool)](this, y)
    else
      @"llvm.uadd.with.overflow.i64"[(USize, Bool)](this, y)
    end

  fun subc(y: USize): (USize, Bool) =>
    ifdef ilp32 then
      @"llvm.usub.with.overflow.i32"[(USize, Bool)](this, y)
    else
      @"llvm.usub.with.overflow.i64"[(USize, Bool)](this, y)
    end

  fun mulc(y: USize): (USize, Bool) =>
    ifdef ilp32 then
      @"llvm.umul.with.overflow.i32"[(USize, Bool)](this, y)
    else
      @"llvm.umul.with.overflow.i64"[(USize, Bool)](this, y)
    end

primitive U128 is _UnsignedInteger[U128]
  new create(value: U128 = 0) => value
  new from[A: (Number & Real[A] val)](a: A) => a.u128()

  new min_value() => 0
  new max_value() => 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U128 =>
    let x = (this - 1).clz()
    1 << (if x == 0 then 0 else bitwidth() - x end)

  fun abs(): U128 => this
  fun bswap(): U128 => @"llvm.bswap.i128"[U128](this)
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
  fun min(y: U128): U128 => if this < y then this else y end
  fun max(y: U128): U128 => if this > y then this else y end
  fun hash(): U64 => ((this >> 64).u64() xor this.u64()).hash()

  fun string(): String iso^ =>
    _ToString._u128(this, false)

  fun mul(y: U128): U128 =>
    ifdef native128 then
      this * y
    else
      let x_hi = (this >> 64).u64()
      let x_lo = this.u64()
      let y_hi = (y >> 64).u64()
      let y_lo = y.u64()

      let mask = U64(0x00000000FFFFFFFF)

      var lo = (x_lo and mask) * (y_lo and mask)
      var t = lo >> 32
      lo = lo and mask
      t = t + ((x_lo >> 32) * (y_lo and mask))
      lo = lo + ((t and mask) << 32)

      var hi = t >> 32
      t = lo >> 32
      lo = lo and mask
      t = t + ((y_lo >> 32) * (x_lo and mask))
      lo = lo + ((t and mask) << 32)
      hi = hi + (t >> 32)
      hi = hi + ((x_lo >> 32) * (y_lo >> 32))
      hi = hi + (x_hi * y_lo) + (x_lo * y_hi)

      (hi.u128() << 64) or lo.u128()
    end

  fun divmod(y: U128): (U128, U128) =>
    ifdef native128 then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var quot: U128 = 0
      var qbit: U128 = 1
      var num = this
      var den = y

      while den.i128() >= 0 do
        den = den << 1
        qbit = qbit << 1
      end

      while qbit != 0 do
        if den <= num then
          num = num - den
          quot = quot + qbit
        end

        den = den >> 1
        qbit = qbit >> 1
      end
      (quot, num)
    end

  fun div(y: U128): U128 =>
    ifdef native128 then
      this / y
    else
      (let q, let r) = divmod(y)
      q
    end

  fun mod(y: U128): U128 =>
    ifdef native128 then
      this % y
    else
      (let q, let r) = divmod(y)
      r
    end

  fun mul_unsafe(y: U128): U128 =>
    """
    Unsafe operation.
    If the operation overflows, the result is undefined.
    """
    ifdef native128 then
      this *~ y
    else
      this * y
    end

  fun divmod_unsafe(y: U128): (U128, U128) =>
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

  fun div_unsafe(y: U128): U128 =>
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

  fun mod_unsafe(y: U128): U128 =>
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
    let v = f64()
    if v > F32.max_value().f64() then
      F32._inf(false)
    else
      v.f32()
    end

  fun f64(): F64 =>
    if this == 0 then
      return 0
    end

    var a = this
    let sd = bitwidth() - clz()
    var e = (sd - 1).u64()

    if sd > 53 then
      match sd
      | 54 => a = a << 1
      | 55 => None
      else
        a = (a >> (sd - 55)) or
          if (a and (-1 >> ((bitwidth() + 55) - sd))) != 0 then 1 else 0 end
      end

      if (a and 4) != 0 then
        a = a or 1
      end

      a = (a + 1) >> 2

      if (a and (1 << 53)) != 0 then
        a = a >> 1
        e = e + 1
      end
    else
      a = a << (53 - sd)
    end

    F64.from_bits(((e + 1023) << 52) or (a.u64() and 0xF_FFFF_FFFF_FFFF))

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

type Unsigned is (U8 | U16 | U32 | U64 | U128 | ULong | USize)
