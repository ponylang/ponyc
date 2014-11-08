interface Any tag

primitive None is Comparable[None], Stringable
  fun box string(): String => "None"

primitive Bool is Comparable[Bool], Logical[Bool], Stringable
  fun box eq(y: Bool box): Bool => this == y
  fun box ne(y: Bool box): Bool => this != y
  fun box and_(y: Bool): Bool => this and y
  fun box or_(y: Bool): Bool => this or y
  fun box xor_(y: Bool): Bool => this xor y
  fun box not_(): Bool => not this

  fun box string(): String => if this then "true" else "false" end
