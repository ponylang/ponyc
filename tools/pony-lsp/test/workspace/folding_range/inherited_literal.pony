trait InheritedLiteral
  fun m(x: U32): Any val =>
    object val
      fun f(): U32 =>
        x
    end

class UsesLiteralA is InheritedLiteral

class UsesLiteralB is InheritedLiteral
