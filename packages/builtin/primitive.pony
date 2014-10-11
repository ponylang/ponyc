type Any is {} tag

primitive None is Stringable
  fun box string(): String => "None"

primitive Bool is Logical[Bool], Stringable
  fun box and_(y: Bool): Bool => compiler_intrinsic
  fun box or_(y: Bool): Bool => compiler_intrinsic
  fun box xor_(y: Bool): Bool => compiler_intrinsic
  fun box not_(): Bool => compiler_intrinsic

  fun box string(): String => if this then "true" else "false" end
