type Any is {}

type None

type True

type False

type Bool is (True | False)

class I8 val is Stringable
  fun val max(that: I8): I8 =>
    if this > that then this else that end

class I16 val is Stringable
  fun val max(that: I16): I16 =>
    if this > that then this else that end

class I32 val is Stringable
  fun val max(that: I32): I32 =>
    if this > that then this else that end

class I64 val is Stringable
  fun val max(that: I64): I64 =>
    if this > that then this else that end

class I128 val is Stringable
  fun val max(that: I128): I128 =>
    if this > that then this else that end

class U8 val is Stringable
  fun val max(that: U8): U8 =>
    if this > that then this else that end

  fun val next_pow2(): U8 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x + 1

class U16 val is Stringable
  fun val max(that: U16): U16 =>
    if this > that then this else that end

  fun val next_pow2(): U16 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x + 1

class U32 val is Stringable
  fun val max(that: U32): U32 =>
    if this > that then this else that end

  fun val next_pow2(): U32 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 3)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x + 1

class U64 val is Stringable
  fun val max(that: U64): U64 =>
    if this > that then this else that end

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

class U128 val is Stringable
  fun val max(that: U128): U128 =>
    if this > that then this else that end

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

class F32 val

class F64 val

class IntLiteral val

class FloatLiteral val

class String val is Stringable, Ordered[String]
  fun box lt(that: String): Bool => compiler_intrinsic

trait Stringable
  fun box string(): String => compiler_intrinsic

type Integer is
  ( IntLiteral
  | I8
  | I16
  | I32
  | I64
  | I128
  | U8
  | U16
  | U32
  | U64
  | U128
  )

type Float is
  ( FloatLiteral
  | F32
  | F64
  )

type Arithmetic is (Integer | Float)
