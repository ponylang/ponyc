primitive U8 is Integer[U8]
  new create(from: U128) => compiler_intrinsic

  fun box max(that: U8): U8 =>
    if this > that then this else that end

  fun box min(that: U8): U8 =>
    if this < that then this else that end

  fun box next_pow2(): U8 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x + 1

  fun box abs(): U8 => this
  fun box bswap(): U8 => this
  fun box popcount(): U8 => @"llvm.ctpop.i8"[U8](this)
  fun box clz(): U8 => @"llvm.ctlz.i8"[U8](this, false)
  fun box ctz(): U8 => @"llvm.cttz.i8"[U8](this, false)
  fun box bitwidth(): U8 => 8

  fun box addc(y: U8): (U8, Bool) =>
    @"llvm.uadd.with.overflow.i8"[(U8, Bool)](this, y)
  fun box subc(y: U8): (U8, Bool) =>
    @"llvm.usub.with.overflow.i8"[(U8, Bool)](this, y)
  fun box mulc(y: U8): (U8, Bool) =>
    @"llvm.umul.with.overflow.i8"[(U8, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    u64().string(fmt, prec, width, align)

primitive U16 is Integer[U16]
  new create(from: U128) => compiler_intrinsic

  fun box max(that: U16): U16 =>
    if this > that then this else that end

  fun box min(that: U16): U16 =>
    if this < that then this else that end

  fun box next_pow2(): U16 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x + 1

  fun box abs(): U16 => this
  fun box bswap(): U16 => @"llvm.bswap.i16"[U16](this)
  fun box popcount(): U16 => @"llvm.ctpop.i16"[U16](this)
  fun box clz(): U16 => @"llvm.ctlz.i16"[U16](this, false)
  fun box ctz(): U16 => @"llvm.cttz.i16"[U16](this, false)
  fun box bitwidth(): U16 => 16

  fun box addc(y: U16): (U16, Bool) =>
    @"llvm.uadd.with.overflow.i16"[(U16, Bool)](this, y)
  fun box subc(y: U16): (U16, Bool) =>
    @"llvm.usub.with.overflow.i16"[(U16, Bool)](this, y)
  fun box mulc(y: U16): (U16, Bool) =>
    @"llvm.umul.with.overflow.i16"[(U16, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    u64().string(fmt, prec, width, align)

primitive U32 is Integer[U32]
  new create(from: U128) => compiler_intrinsic

  fun box max(that: U32): U32 =>
    if this > that then this else that end

  fun box min(that: U32): U32 =>
    if this < that then this else that end

  fun box next_pow2(): U32 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x + 1

  fun box abs(): U32 => this
  fun box bswap(): U32 => @"llvm.bswap.i32"[U32](this)
  fun box popcount(): U32 => @"llvm.ctpop.i32"[U32](this)
  fun box clz(): U32 => @"llvm.ctlz.i32"[U32](this, false)
  fun box ctz(): U32 => @"llvm.cttz.i32"[U32](this, false)
  fun box bitwidth(): U32 => 32

  fun box addc(y: U32): (U32, Bool) =>
    @"llvm.uadd.with.overflow.i32"[(U32, Bool)](this, y)
  fun box subc(y: U32): (U32, Bool) =>
    @"llvm.usub.with.overflow.i32"[(U32, Bool)](this, y)
  fun box mulc(y: U32): (U32, Bool) =>
    @"llvm.umul.with.overflow.i32"[(U32, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    u64().string(fmt, prec, width, align)

primitive U64 is Integer[U64]
  new create(from: U128) => compiler_intrinsic

  fun box max(that: U64): U64 =>
    if this > that then this else that end

  fun box min(that: U64): U64 =>
    if this < that then this else that end

  fun box next_pow2(): U64 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x + 1

  fun box abs(): U64 => this
  fun box bswap(): U64 => @"llvm.bswap.i64"[U64](this)
  fun box popcount(): U64 => @"llvm.ctpop.i64"[U64](this)
  fun box clz(): U64 => @"llvm.ctlz.i64"[U64](this, false)
  fun box ctz(): U64 => @"llvm.cttz.i64"[U64](this, false)
  fun box bitwidth(): U64 => 64

  fun box addc(y: U64): (U64, Bool) =>
    @"llvm.uadd.with.overflow.i64"[(U64, Bool)](this, y)
  fun box subc(y: U64): (U64, Bool) =>
    @"llvm.usub.with.overflow.i64"[(U64, Bool)](this, y)
  fun box mulc(y: U64): (U64, Bool) =>
    @"llvm.umul.with.overflow.i64"[(U64, Bool)](this, y)

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    // TODO: align
    var table = IntTable.large()

    var base = match fmt
    | IntUTF32 => return recover String.append_utf32(u32()) end
    | IntBinary => I64(2)
    | IntOctal => I64(8)
    | IntHex => I64(16)
    | IntHexSmall => table = IntTable.small(); I64(16)
    else
      I64(10)
    end

    recover
      var s = String.reserve(prec.max(width.max(31)))
      var value = this
      let div = base.u64()
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / div
          let index = tmp - (value * div)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end

        while i < prec.i64() do
          s.append_byte('0')
          i = i + 1
        end

        while i < width.i64() do
          s.append_byte(' ')
          i = i + 1
        end
      end

      s.reverse_in_place()
    end

primitive U128 is Integer[U128]
  new create(from: U128) => compiler_intrinsic

  fun box max(that: U128): U128 =>
    if this > that then this else that end

  fun box min(that: U128): U128 =>
    if this < that then this else that end

  fun box next_pow2(): U128 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x = x or (x >> 64)
    x + 1

  fun box abs(): U128 => this
  fun box bswap(): U128 => @"llvm.bswap.i128"[U128](this)
  fun box popcount(): U128 => @"llvm.ctpop.i128"[U128](this)
  fun box clz(): U128 => @"llvm.ctlz.i128"[U128](this, false)
  fun box ctz(): U128 => @"llvm.cttz.i128"[U128](this, false)
  fun box bitwidth(): U128 => 128

  fun box string(fmt: IntFormat = FormatDefault, prec: U64 = 1,
    width: U64 = 0, align: Align = AlignRight): String iso^
  =>
    // TODO: align
    var table = IntTable.large()

    var base = match fmt
    | IntUTF32 => return recover String.append_utf32(u32()) end
    | IntBinary => I64(2)
    | IntOctal => I64(8)
    | IntHex => I64(16)
    | IntHexSmall => table = IntTable.small(); I64(16)
    else
      I64(10)
    end

    recover
      var s = String.reserve(prec.max(width.max(31)))
      var value = this
      let div = base.u128()
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / div
          let index = tmp - (value * div)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end

        while i < prec.i64() do
          s.append_byte('0')
          i = i + 1
        end

        while i < width.i64() do
          s.append_byte(' ')
          i = i + 1
        end
      end

      s.reverse_in_place()
    end

  fun box divmod(y: U128): (U128, U128) =>
    if Platform.has_i128() then
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

  fun box div(y: U128): U128 =>
    if Platform.has_i128() then
      this / y
    else
      let (q, r) = divmod(y)
      q
    end

  fun box mod(y: U128): U128 =>
    if Platform.has_i128() then
      this % y
    else
      let (q, r) = divmod(y)
      r
    end

  fun box f32(): F32 => this.f64().f32()

  fun box f64(): F64 =>
    let low = this.u64()
    let high = (this >> 64).u64()
    let x = low.f64()
    let y = high.f64() * (U128(1) << 64).f64()
    x + y
