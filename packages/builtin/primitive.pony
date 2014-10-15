type Any is {} tag

primitive None is Comparable[None], Stringable
  fun box string(): String => "None"

primitive Bool is Comparable[Bool], Logical[Bool], Stringable
  fun box eq(y: Bool box): Bool => compiler_intrinsic
  fun box ne(y: Bool box): Bool => compiler_intrinsic
  fun box and_(y: Bool): Bool => compiler_intrinsic
  fun box or_(y: Bool): Bool => compiler_intrinsic
  fun box xor_(y: Bool): Bool => compiler_intrinsic
  fun box not_(): Bool => compiler_intrinsic

  fun box string(): String => if this then "true" else "false" end
