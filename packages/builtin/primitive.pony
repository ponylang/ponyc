trait Any infer

class None

class True

class False

alias Bool:(True|False)

alias Number:(Integer|Real)

alias Integer:
  (I8
  |I16
  |I32
  |I64
  |I128
  |Unsigned
  )

alias Unsigned:
  (U8
  |U16
  |U32
  |U64
  |U128
  |IntLiteral
  )

alias Real:(F32|F64|NumLiteral)

alias NumLiteral:(IntLiteral|FloatLiteral)

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

trait Stringable infer
  fun string{iso|var|val}():String
