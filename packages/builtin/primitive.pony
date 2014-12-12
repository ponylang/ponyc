interface Any tag

primitive None is Stringable
  fun box string(fmt: FormatDefault = FormatDefault, prec: U64 = -1,
    width: U64 = 0, align: Align = AlignLeft): String iso^
  =>
    "None".string(fmt, prec, width, align)

primitive Bool is Logical[Bool], Stringable
  fun box eq(y: Bool box): Bool => this == y
  fun box ne(y: Bool box): Bool => this != y
  fun box op_and(y: Bool): Bool => this and y
  fun box op_or(y: Bool): Bool => this or y
  fun box op_xor(y: Bool): Bool => this xor y
  fun box op_not(): Bool => not this

  fun box string(fmt: FormatDefault = FormatDefault, prec: U64 = -1,
    width: U64 = 0, align: Align = AlignLeft): String iso^
  =>
    (if this then "true" else "false" end).string(fmt, prec, width, align)
