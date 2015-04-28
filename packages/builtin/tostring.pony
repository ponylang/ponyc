interface Stringable box
  """
  Things that can be turned into a String.
  """
  fun string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^

primitive FormatDefault
primitive PrefixDefault
primitive AlignLeft
primitive AlignRight
primitive AlignCenter

type Align is
  ( AlignLeft
  | AlignRight
  | AlignCenter)

primitive IntUTF32
primitive IntBinary
primitive IntBinaryBare
primitive IntOctal
primitive IntOctalBare
primitive IntHex
primitive IntHexBare
primitive IntHexSmall
primitive IntHexSmallBare

type IntFormat is
  ( FormatDefault
  | IntUTF32
  | IntBinary
  | IntBinaryBare
  | IntOctal
  | IntOctalBare
  | IntHex
  | IntHexBare
  | IntHexSmall
  | IntHexSmallBare)

primitive NumberSpacePrefix
primitive NumberSignPrefix

type NumberPrefix is
  ( PrefixDefault
  | NumberSpacePrefix
  | NumberSignPrefix)

primitive FloatExp
primitive FloatExpLarge
primitive FloatFix
primitive FloatFixLarge
primitive FloatGeneral
primitive FloatGeneralLarge

type FloatFormat is
  ( FormatDefault
  | FloatExp
  | FloatExpLarge
  | FloatFix
  | FloatFixLarge
  | FloatGeneral
  | FloatGeneralLarge)

primitive ToString
  fun _large(): String => "0123456789ABCDEF"
  fun _small(): String => "0123456789abcdef"

  fun _fmt_int(fmt: IntFormat): (U64, String, String) =>
    match fmt
    | IntBinary => (2, "b0", _large())
    | IntBinaryBare => (2, "", _large())
    | IntOctal => (8, "o0", _large())
    | IntOctalBare => (8, "", _large())
    | IntHex => (16, "x0", _large())
    | IntHexBare => (16, "", _large())
    | IntHexSmall => (16, "x0", _small())
    | IntHexSmallBare => (16, "", _small())
    else
      (10, "", _large())
    end

  fun _prefix(neg: Bool, prefix: NumberPrefix): String =>
    if neg then
      "-"
    else
      match prefix
      | NumberSpacePrefix => " "
      | NumberSignPrefix => "+"
      else
        ""
      end
    end

  fun _extend_digits(s: String ref, digits: U64) =>
    while s.size() < digits do
      s.append("0")
    end

  fun _pad(s: String ref, width: U64, align: Align, fill: U32) =>
    var pre: U64 = 0
    var post: U64 = 0

    if s.size() < width then
      let rem = width - s.size()
      let fills = String.from_utf32(fill)

      match align
      | AlignLeft => post = rem
      | AlignRight => pre = rem
      | AlignCenter => pre = rem / 2; post = rem - pre
      end

      while pre > 0 do
        s.append(fills)
        pre = pre - 1
      end

      s.reverse_in_place()

      while post > 0 do
        s.append(fills)
        post = post - 1
      end
    else
      s.reverse_in_place()
    end

  fun _u64(x: U64, neg: Bool, fmt: IntFormat, prefix: NumberPrefix,
    prec: U64, width: U64, align: Align, fill: U32): String iso^
  =>
    match fmt
    | IntUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base: U64, var typestring: String, var table: String) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)

    recover
      var s = String((prec + 1).max(width.max(31)))
      var value = x
      var i: I64 = 0

      try
        while value != 0 do
          let index = ((value = value / base) - (value * base))
          s.push(table(index))
          i = i + 1
        end
      end

      s.append(typestring)
      _extend_digits(s, prec)
      s.append(prestring)
      _pad(s, width, align, fill)
      s
    end

  fun _u128(x: U128, neg: Bool, fmt: IntFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    match fmt
    | IntUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base': U64, var typestring: String, var table: String) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)
    var base = base'.u128()

    recover
      var s = String((prec + 1).max(width.max(31)))
      var value = x
      var i: I64 = 0

      try
        while value != 0 do
          let index = ((value = value / base) - (value * base)).u64()
          s.push(table(index))
          i = i + 1
        end
      end

      s.append(typestring)
      _extend_digits(s, prec)
      s.append(prestring)
      _pad(s, width, align, fill)
      s
    end

  fun _f64(x: F64, fmt: FloatFormat = FormatDefault,
    prefix: NumberPrefix = PrefixDefault, prec: U64 = 6, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    // TODO: prefix, align, fill
    recover
      var s = String((prec + 8).max(width.max(31)))
      var f = String(31).append("%")

      if width > 0 then f.append(width.string()) end
      f.append(".").append(prec.string())

      match fmt
      | FloatExp => f.append("e")
      | FloatExpLarge => f.append("E")
      | FloatFix => f.append("f")
      | FloatFixLarge => f.append("F")
      | FloatGeneral => f.append("g")
      | FloatGeneralLarge => f.append("G")
      else
        f.append("g")
      end

      if Platform.windows() then
        @_snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      else
        @snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      end

      s.recalc()
    end
