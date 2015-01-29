interface Any tag

primitive None is Stringable
  fun string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    "None".string(fmt, prefix, prec, width, align, fill)

primitive Bool is Logical[Bool], Comparable[Bool], Stringable
  new create(from: Bool) => compiler_intrinsic

  fun eq(y: Bool): Bool => this == y
  fun ne(y: Bool): Bool => this != y
  fun op_and(y: Bool): Bool => this and y
  fun op_or(y: Bool): Bool => this or y
  fun op_xor(y: Bool): Bool => this xor y
  fun op_not(): Bool => not this

  fun string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    (if this then "true" else "false" end).string(fmt, prefix, prec, width,
      align, fill)
