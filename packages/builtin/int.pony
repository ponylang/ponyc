primitive I8 is Stringable, ArithmeticConvertible
  new create(from: I128) => compiler_intrinsic

  fun tag max(that: I8): I8 =>
    if this > that then this else that end

  fun tag min(that: I8): I8 =>
    if this < that then this else that end

  fun tag abs(): I8 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i8(this, 10) end

primitive I16 is Stringable, ArithmeticConvertible
  new create(from: I128) => compiler_intrinsic

  fun tag max(that: I16): I16 =>
    if this > that then this else that end

  fun tag min(that: I16): I16 =>
    if this < that then this else that end

  fun tag abs(): I16 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i16(this, 10) end

primitive I32 is Stringable, ArithmeticConvertible
  new create(from: I128) => compiler_intrinsic

  fun tag max(that: I32): I32 =>
    if this > that then this else that end

  fun tag min(that: I32): I32 =>
    if this < that then this else that end

  fun tag abs(): I32 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i32(this, 10) end

primitive I64 is Stringable, ArithmeticConvertible
  new create(from: I128) => compiler_intrinsic

  fun tag max(that: I64): I64 =>
    if this > that then this else that end

  fun tag min(that: I64): I64 =>
    if this < that then this else that end

  fun tag abs(): I64 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i64(this, 10) end

primitive I128 is Stringable, ArithmeticConvertible
  new create(from: I128) => compiler_intrinsic

  fun tag max(that: I128): I128 =>
    if this > that then this else that end

  fun tag min(that: I128): I128 =>
    if this < that then this else that end

  fun tag abs(): I128 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i128(this, 10) end

  fun tag divmod(y: I128): (I128, I128) =>
    if Platform.has_i128() then
      (this / y, this % y)
    else
      if y == 0 then
        // TODO: returning (0, 0) causes a codegen error
        var qr: (I128, I128) = (0, 0)
        return qr
      end

      var num: I128 = this
      var den: I128 = y

      var minus = if num < 0 then
        num = -num
        True
      else
        False
      end

      if den < 0 then
        den = -den
        minus = not minus
      end

      var (q, r) = num.u128().divmod(den.u128())
      var (q', r') = (q.i128(), r.i128())

      if minus then
        q' = -q'
      end

      (q', r')
    end

  fun tag div(y: I128): I128 =>
    if Platform.has_i128() then
      this / y
    else
      var (q, r) = divmod(y)
      q
    end

  fun tag mod(y: I128): I128 =>
    if Platform.has_i128() then
      this % y
    else
      var (q, r) = divmod(y)
      r
    end

  fun tag f32(): F32 => this.f64().f32()

  fun tag f64(): F64 =>
    if this < 0 then
      -(-this).f64()
    else
      this.u128().f64()
    end
