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
  fun box popcount(): U8 => @llvm.ctpop.i8[U8](this)
  fun box clz(): U8 => @llvm.ctlz.i8[U8](this, false)
  fun box ctz(): U8 => @llvm.cttz.i8[U8](this, false)

  fun box string(): String iso^ => recover String.from_u8(this, 10) end

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
  fun box bswap(): U16 => @llvm.bswap.i16[U16](this)
  fun box popcount(): U16 => @llvm.ctpop.i16[U16](this)
  fun box clz(): U16 => @llvm.ctlz.i16[U16](this, false)
  fun box ctz(): U16 => @llvm.cttz.i16[U16](this, false)

  fun box string(): String iso^ => recover String.from_u16(this, 10) end

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
  fun box bswap(): U32 => @llvm.bswap.i32[U32](this)
  fun box popcount(): U32 => @llvm.ctpop.i32[U32](this)
  fun box clz(): U32 => @llvm.ctlz.i32[U32](this, false)
  fun box ctz(): U32 => @llvm.cttz.i32[U32](this, false)

  fun box string(): String iso^ => recover String.from_u32(this, 10) end

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
  fun box bswap(): U64 => @llvm.bswap.i64[U64](this)
  fun box popcount(): U64 => @llvm.ctpop.i64[U64](this)
  fun box clz(): U64 => @llvm.ctlz.i64[U64](this, false)
  fun box ctz(): U64 => @llvm.cttz.i64[U64](this, false)

  fun box string(): String iso^ => recover String.from_u64(this, 10) end

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
  fun box bswap(): U128 => @llvm.bswap.i128[U128](this)
  fun box popcount(): U128 => @llvm.ctpop.i128[U128](this)
  fun box clz(): U128 => @llvm.ctlz.i128[U128](this, false)
  fun box ctz(): U128 => @llvm.cttz.i128[U128](this, false)

  fun box string(): String iso^ => recover String.from_u128(this, 10) end

  fun box divmod(y: U128): (U128, U128) =>
    if Platform.has_i128() then
      (this / y, this % y)
    else
      if y == 0 then
        // TODO: returning (0, 0) causes a codegen error
        var qr: (U128, U128) = (0, 0)
        return qr
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
      var (q, r) = divmod(y)
      q
    end

  fun box mod(y: U128): U128 =>
    if Platform.has_i128() then
      this % y
    else
      var (q, r) = divmod(y)
      r
    end

  fun box f32(): F32 => this.f64().f32()

  fun box f64(): F64 =>
    var low = this.u64()
    var high = (this >> 64).u64()
    var x = low.f64()
    var y = high.f64() * (U128(1) << 64).f64()
    x + y
