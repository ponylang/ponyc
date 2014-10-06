type Any is {} tag

primitive None is Stringable
  fun tag string(): String => "None"

primitive Bool is Logical[Bool], Stringable
  fun tag and_(y: Bool): Bool => compiler_intrinsic
  fun tag or_(y: Bool): Bool => compiler_intrinsic
  fun tag xor_(y: Bool): Bool => compiler_intrinsic
  fun tag not_(): Bool => compiler_intrinsic

  fun tag string(): String => if this then "true" else "false" end
