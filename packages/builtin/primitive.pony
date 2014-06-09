trait Any

type None

type True

type False

type Bool is (True | False)

class I8

class I16

class I32

class I64

class I128

class U8

class U16

class U32

class U64

class U128

class F32

class F64

class IntLiteral

class FloatLiteral

class String

trait Stringable
  fun box string(): String

type Arithmetic is
  ( I8
  | I16
  | I32
  | I64
  | I128
  | U16
  | U32
  | U64
  | U128
  | F32
  | F64
  )
