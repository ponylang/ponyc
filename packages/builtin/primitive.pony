type Any is {} tag

class None val is Stringable
  fun box string(): String => "None"

class True val is Stringable
  fun box string(): String => "True"

class False val is Stringable
  fun box string(): String => "False"

type Bool is (True | False)

class I8 val is Stringable
  fun val max(that: I8): I8 =>
    if this > that then this else that end

  fun box string(): String => compiler_intrinsic

class I16 val is Stringable
  fun val max(that: I16): I16 =>
    if this > that then this else that end

  fun box string(): String => compiler_intrinsic

class I32 val is Stringable
  fun val max(that: I32): I32 =>
    if this > that then this else that end

  fun box string(): String => compiler_intrinsic

class I64 val is Stringable
  fun val max(that: I64): I64 =>
    if this > that then this else that end

  fun box string(): String => compiler_intrinsic

class I128 val is Stringable
  fun val max(that: I128): I128 =>
    if this > that then this else that end

  fun box string(): String => compiler_intrinsic

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

  fun box string(): String => compiler_intrinsic

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

  fun box string(): String => compiler_intrinsic

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

  fun box string(): String => compiler_intrinsic

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

  fun box string(): String => compiler_intrinsic

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

  fun box string(): String => compiler_intrinsic

class F16 val is Stringable
  fun box string(): String => compiler_intrinsic

class F32 val is Stringable
  fun box string(): String => compiler_intrinsic

class F64 val is Stringable
  fun box string(): String => compiler_intrinsic

class IntLiteral val is Stringable
  fun box string(): String => compiler_intrinsic

class FloatLiteral val is Stringable
  fun box string(): String => compiler_intrinsic

class String val is Ordered[String]
  fun box lt(that: String box): Bool => compiler_intrinsic
  /*fun box string(): String => this*/

trait Stringable
  fun box string(): String

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
