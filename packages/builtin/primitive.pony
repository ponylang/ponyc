trait Any infer

class None

class True

class False

alias Bool:(True|False)

trait Number
  fun string{iso|var|val}():String

trait Integer is Number

trait Real is Number

class I8 is Integer

class I16 is Integer

class I32 is Integer

class I64 is Integer

class U8 is Integer

class U16 is Integer

class U32 is Integer

class U64 is Integer

class F32 is Real

class F64 is Real

class String
