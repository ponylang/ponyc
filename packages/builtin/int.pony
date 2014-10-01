primitive I8 is Stringable, ArithmeticConvertible
  fun tag max(that: I8): I8 =>
    if this > that then this else that end

  fun tag min(that: I8): I8 =>
    if this < that then this else that end

  fun tag abs(): I8 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i8(this, 10) end

primitive I16 is Stringable, ArithmeticConvertible
  fun tag max(that: I16): I16 =>
    if this > that then this else that end

  fun tag min(that: I16): I16 =>
    if this < that then this else that end

  fun tag abs(): I16 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i16(this, 10) end

primitive I32 is Stringable, ArithmeticConvertible
  fun tag max(that: I32): I32 =>
    if this > that then this else that end

  fun tag min(that: I32): I32 =>
    if this < that then this else that end

  fun tag abs(): I32 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i32(this, 10) end

primitive I64 is Stringable, ArithmeticConvertible
  fun tag max(that: I64): I64 =>
    if this > that then this else that end

  fun tag min(that: I64): I64 =>
    if this < that then this else that end

  fun tag abs(): I64 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i64(this, 10) end

primitive I128 is Stringable, ArithmeticConvertible
  fun tag max(that: I128): I128 =>
    if this > that then this else that end

  fun tag min(that: I128): I128 =>
    if this < that then this else that end

  fun tag abs(): I128 => if this < 0 then -this else this end

  fun tag string(): String iso^ => recover String.from_i128(this, 10) end
