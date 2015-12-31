primitive U8 is _UnsignedInteger[U8]
  new create(value: U8 = 0) => compile_intrinsic
  fun tag from[B: (Number & Real[B] val)](a: B): U8 => a.u8()

  fun tag min_value(): U8 => 0
  fun tag max_value(): U8 => 0xFF

  fun next_pow2(): U8 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x + 1

  fun abs(): U8 => this
  fun bswap(): U8 => this
  fun popcount(): U8 => @"llvm.ctpop.i8"[U8](this)
  fun clz(): U8 => @"llvm.ctlz.i8"[U8](this, false)
  fun ctz(): U8 => @"llvm.cttz.i8"[U8](this, false)
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
  new create(value: U16 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U16 => a.u16()

  fun tag min_value(): U16 => 0
  fun tag max_value(): U16 => 0xFFFF

  fun next_pow2(): U16 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x + 1

  fun abs(): U16 => this
  fun bswap(): U16 => @"llvm.bswap.i16"[U16](this)
  fun popcount(): U16 => @"llvm.ctpop.i16"[U16](this)
  fun clz(): U16 => @"llvm.ctlz.i16"[U16](this, false)
  fun ctz(): U16 => @"llvm.cttz.i16"[U16](this, false)
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
  new create(value: U32 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U32 => a.u32()

  fun tag min_value(): U32 => 0
  fun tag max_value(): U32 => 0xFFFF_FFFF

  fun next_pow2(): U32 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x + 1

  fun abs(): U32 => this
  fun bswap(): U32 => @"llvm.bswap.i32"[U32](this)
  fun popcount(): U32 => @"llvm.ctpop.i32"[U32](this)
  fun clz(): U32 => @"llvm.ctlz.i32"[U32](this, false)
  fun ctz(): U32 => @"llvm.cttz.i32"[U32](this, false)
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
  new create(value: U64 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U64 => a.u64()

  fun tag min_value(): U64 => 0
  fun tag max_value(): U64 => 0xFFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U64 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x + 1

  fun abs(): U64 => this
  fun bswap(): U64 => @"llvm.bswap.i64"[U64](this)
  fun popcount(): U64 => @"llvm.ctpop.i64"[U64](this)
  fun clz(): U64 => @"llvm.ctlz.i64"[U64](this, false)
  fun ctz(): U64 => @"llvm.cttz.i64"[U64](this, false)
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
  new create(value: ULong = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): ULong => a.ulong()

  fun tag min_value(): ULong => 0

  fun tag max_value(): ULong =>
    ifdef ilp32 or llp64 then
      0xFFFF_FFFF
    else
      0xFFFF_FFFF_FFFF_FFFF
    end

  fun next_pow2(): ULong =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)

    ifdef lp64 then
      x = x or (x >> 32)
    end

    x + 1

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
  new create(value: USize = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): USize => a.usize()

  fun tag min_value(): USize => 0

  fun tag max_value(): USize =>
    ifdef ilp32 then
      0xFFFF_FFFF
    else
      0xFFFF_FFFF_FFFF_FFFF
    end

  fun next_pow2(): USize =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)

    ifdef llp64 or lp64 then
      x = x or (x >> 32)
    end

    x + 1

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
  new create(value: U128 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U128 => a.u128()

  fun tag min_value(): U128 => 0
  fun tag max_value(): U128 => 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U128 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x = x or (x >> 64)
    x + 1

  fun abs(): U128 => this
  fun bswap(): U128 => @"llvm.bswap.i128"[U128](this)
  fun popcount(): U128 => @"llvm.ctpop.i128"[U128](this)
  fun clz(): U128 => @"llvm.ctlz.i128"[U128](this, false)
  fun ctz(): U128 => @"llvm.cttz.i128"[U128](this, false)
  fun bitwidth(): U128 => 128
  fun min(y: U128): U128 => if this < y then this else y end
  fun max(y: U128): U128 => if this > y then this else y end
  fun hash(): U64 => ((this >> 64).u64() xor this.u64()).hash()

  fun string(fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: USize = 1, width: USize = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    _ToString._u128(this, false, fmt, prefix, prec, width, align, fill)

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

  fun f32(): F32 =>
    f64().f32()

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
