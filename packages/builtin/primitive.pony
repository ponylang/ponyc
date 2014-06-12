type Any is {}

type None val

type True val

type False val

type Bool val is (True | False)

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

type Arithmetic val is
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
