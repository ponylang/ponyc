interface tag Any


primitive None is Stringable
  fun string(fmt: FormatSettings = FormatSettingsDefault): String iso^
  =>
    "None".string(fmt)

primitive Bool is Stringable
  new create(from: Bool) => from

  fun eq(y: Bool): Bool => this == y
  fun ne(y: Bool): Bool => this != y
  fun op_and(y: Bool): Bool => this and y
  fun op_or(y: Bool): Bool => this or y
  fun op_xor(y: Bool): Bool => this xor y
  fun op_not(): Bool => not this

  fun string(fmt: FormatSettings = FormatSettingsDefault): String iso^
  =>
    (if this then "true" else "false" end).string(fmt)
