type Any is {} tag

primitive None is Stringable
  fun box string(): String => "None"

primitive True is Stringable
  fun box string(): String => "True"

primitive False is Stringable
  fun box string(): String => "False"

type Bool is (True | False)

primitive I8 is Stringable
  fun val max(that: I8): I8 =>
    if this > that then this else that end

  fun val min(that: I8): I8 =>
    if this < that then this else that end

  fun box string(): String => "I8"

primitive I16 is Stringable
  fun val max(that: I16): I16 =>
    if this > that then this else that end

  fun val min(that: I16): I16 =>
    if this < that then this else that end

  fun box string(): String => "I16"

primitive I32 is Stringable
  fun val max(that: I32): I32 =>
    if this > that then this else that end

  fun val min(that: I32): I32 =>
    if this < that then this else that end

  fun box string(): String => "I32"

primitive I64 is Stringable
  fun val max(that: I64): I64 =>
    if this > that then this else that end

  fun val min(that: I64): I64 =>
    if this < that then this else that end

  fun box string(): String => "I64"

primitive I128 is Stringable
  fun val max(that: I128): I128 =>
    if this > that then this else that end

  fun val min(that: I128): I128 =>
    if this < that then this else that end

  fun box string(): String => "I128"

primitive U8 is Stringable
  fun val max(that: U8): U8 =>
    if this > that then this else that end

  fun val min(that: U8): U8 =>
    if this < that then this else that end

  fun val next_pow2(): U8 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x + 1

  fun box string(): String => "U8"

primitive U16 is Stringable
  fun val max(that: U16): U16 =>
    if this > that then this else that end

  fun val min(that: U16): U16 =>
    if this < that then this else that end

  fun val next_pow2(): U16 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x + 1

  fun box string(): String => "U16"

primitive U32 is Stringable
  fun val max(that: U32): U32 =>
    if this > that then this else that end

  fun val min(that: U32): U32 =>
    if this < that then this else that end

  fun val next_pow2(): U32 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x + 1

  fun box string(): String => "U32"

primitive U64 is Stringable
  fun val max(that: U64): U64 =>
    if this > that then this else that end

  fun val min(that: U64): U64 =>
    if this < that then this else that end

  fun val next_pow2(): U64 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x + 1

  fun box string(): String => "U64"

primitive U128 is Stringable
  fun val max(that: U128): U128 =>
    if this > that then this else that end

  fun val min(that: U128): U128 =>
    if this < that then this else that end

  fun val next_pow2(): U128 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x = x or (x >> 64)
    x + 1

  fun box string(): String => "U128"

primitive F16 is Stringable
  fun box string(): String => "F16"

primitive F32 is Stringable
  fun box string(): String => "F32"

primitive F64 is Stringable
  fun box string(): String => "F64"

primitive SIntLiteral is Stringable
  fun box string(): String => "SIntLiteral"

primitive UIntLiteral is Stringable
  fun box string(): String => "UIntLiteral"

primitive FloatLiteral is Stringable
  fun box string(): String => "FloatLiteral"

type SInt is
  ( SIntLiteral
  | I8
  | I16
  | I32
  | I64
  | I128
  )

type UInt is
  ( UIntLiteral
  | U8
  | U16
  | U32
  | U64
  | U128
  )

type Integer is
  ( SInt
  | UInt
  )

type Float is
  ( FloatLiteral
  | F16
  | F32
  | F64
  )

type Arithmetic is (Integer | Float)
