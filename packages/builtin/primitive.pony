trait Any infer

class None

class True

class False

alias Bool:(True|False)

trait Number
  fun string{iso|var|val}():String

trait Integer is Number

trait Unsigned is Integer

trait Real is Number

class I8 is Integer

class I16 is Integer

class I32 is Integer

class I64 is Integer

class U8 is Unsigned

class U16 is Unsigned

class U32 is Unsigned

class U64 is Unsigned

class F32 is Real

class F64 is Real

class String

trait Stringable infer
  fun string{iso|var|val}():String
