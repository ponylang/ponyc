primitive U8 is Stringable, ArithmeticConvertible
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

  fun box string(): String => "U8"

primitive U16 is Stringable, ArithmeticConvertible
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

  fun box string(): String => "U16"

primitive U32 is Stringable, ArithmeticConvertible
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

  fun box string(): String => "U32"

primitive U64 is Stringable, ArithmeticConvertible
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

  fun box string(): String => "U64"

primitive U128 is Stringable, ArithmeticConvertible
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

  fun box string(): String => "U128"
