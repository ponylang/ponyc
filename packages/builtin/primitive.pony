interface Any tag

primitive None is Comparable[None], Stringable
  fun box string(): String => "None"

primitive Bool is Comparable[Bool], Logical[Bool], Stringable
  fun box eq(y: Bool box): Bool => this == y
  fun box ne(y: Bool box): Bool => this != y
  fun box op_and(y: Bool): Bool => this and y
  fun box op_or(y: Bool): Bool => this or y
  fun box op_xor(y: Bool): Bool => this xor y
  fun box op_not(): Bool => not this

  fun box string(): String => if this then "true" else "false" end
