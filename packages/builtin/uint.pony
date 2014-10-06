primitive U8 is Stringable, ArithmeticConvertible
  new create(from: U128) => compiler_intrinsic

  fun tag max(that: U8): U8 =>
    if this > that then this else that end

  fun tag min(that: U8): U8 =>
    if this < that then this else that end

  fun tag next_pow2(): U8 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x + 1

  fun tag string(): String iso^ => recover String.from_u8(this, 10) end

primitive U16 is Stringable, ArithmeticConvertible
  new create(from: U128) => compiler_intrinsic

  fun tag max(that: U16): U16 =>
    if this > that then this else that end

  fun tag min(that: U16): U16 =>
    if this < that then this else that end

  fun tag next_pow2(): U16 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x + 1

  fun tag string(): String iso^ => recover String.from_u16(this, 10) end

primitive U32 is Stringable, ArithmeticConvertible
  new create(from: U128) => compiler_intrinsic

  fun tag max(that: U32): U32 =>
    if this > that then this else that end

  fun tag min(that: U32): U32 =>
    if this < that then this else that end

  fun tag next_pow2(): U32 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x + 1

  fun tag string(): String iso^ => recover String.from_u32(this, 10) end

primitive U64 is Stringable, ArithmeticConvertible
  new create(from: U128) => compiler_intrinsic

  fun tag max(that: U64): U64 =>
    if this > that then this else that end

  fun tag min(that: U64): U64 =>
    if this < that then this else that end

  fun tag next_pow2(): U64 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x + 1

  fun tag string(): String iso^ => recover String.from_u64(this, 10) end

primitive U128 is Stringable, ArithmeticConvertible
  new create(from: U128) => compiler_intrinsic

  fun tag max(that: U128): U128 =>
    if this > that then this else that end

  fun tag min(that: U128): U128 =>
    if this < that then this else that end

  fun tag next_pow2(): U128 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x = x or (x >> 64)
    x + 1

  fun tag string(): String iso^ => recover String.from_u128(this, 10) end

  fun tag divmod(y: U128): (U128, U128) =>
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

  fun tag div(y: U128): U128 =>
    if Platform.has_i128() then
      this / y
    else
      var (q, r) = divmod(y)
      q
    end

  fun tag mod(y: U128): U128 =>
    if Platform.has_i128() then
      this % y
    else
      var (q, r) = divmod(y)
      r
    end

  fun tag f32(): F32 => this.f64().f32()

  fun tag f64(): F64 =>
    var low = this.u64()
    var high = (this >> 64).u64()
    var x = low.f64()
    var y = high.f64() * (1 << 64)
    x + y
