type Any is {}

type None

type True

type False

type Bool is (True | False)

class I8 val

class I16 val

class I32 val

class I64 val

class I128 val

class U8 val

class U16 val

class U32 val

class U64 val

class U128 val

class F32 val

class F64 val

class IntLiteral val

class FloatLiteral val

class String val

trait Stringable
  fun box string(): String

type Integer is
  ( IntLiteral
  | I8
  | I16
  | I32
  | I64
  | I128
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
