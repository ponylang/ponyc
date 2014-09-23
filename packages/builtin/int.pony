primitive I8 is Stringable, ArithmeticConvertible
  fun val max(that: I8): I8 =>
    if this > that then this else that end

  fun val min(that: I8): I8 =>
    if this < that then this else that end

  fun val abs(): I8 => if this < 0 then -this else this end

  fun box string(): String => "I8"

primitive I16 is Stringable, ArithmeticConvertible
  fun val max(that: I16): I16 =>
    if this > that then this else that end

  fun val min(that: I16): I16 =>
    if this < that then this else that end

  fun val abs(): I16 => if this < 0 then -this else this end

  fun box string(): String => "I16"

primitive I32 is Stringable, ArithmeticConvertible
  fun val max(that: I32): I32 =>
    if this > that then this else that end

  fun val min(that: I32): I32 =>
    if this < that then this else that end

  fun val abs(): I32 => if this < 0 then -this else this end

  fun box string(): String => "I32"

primitive I64 is Stringable, ArithmeticConvertible
  fun val max(that: I64): I64 =>
    if this > that then this else that end

  fun val min(that: I64): I64 =>
    if this < that then this else that end

  fun val abs(): I64 => if this < 0 then -this else this end

  fun box string(): String => "I64"

primitive I128 is Stringable, ArithmeticConvertible
  fun val max(that: I128): I128 =>
    if this > that then this else that end

  fun val min(that: I128): I128 =>
    if this < that then this else that end

  fun val abs(): I128 => if this < 0 then -this else this end

  fun box string(): String => "I128"
