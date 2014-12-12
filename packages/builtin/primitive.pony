interface Any tag

primitive None is Stringable
  fun box string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U8 = ' '): String iso^
  =>
    "None".string(fmt, prefix, prec, width, align, fill)

primitive Bool is Logical[Bool], Stringable
  fun box eq(y: Bool box): Bool => this == y
  fun box ne(y: Bool box): Bool => this != y
  fun box op_and(y: Bool): Bool => this and y
  fun box op_or(y: Bool): Bool => this or y
  fun box op_xor(y: Bool): Bool => this xor y
  fun box op_not(): Bool => not this

  fun box string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U8 = ' '): String iso^
  =>
    (if this then "true" else "false" end).string(fmt, prefix, prec, width,
      align, fill)
