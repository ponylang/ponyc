interface Stringable box
  """
  Things that can be turned into a String.
  """
  fun string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
    """
    Generate a string representation of this object.

    * fmt. Format to use. Since this must be valid for all Stringable types
    this must be set to FormatDefault.
    * prefix. Prefix to use. Since this must be valid for all Stringable types
    this must be set to PrefixDefault.
    * prec. Precision to use. The exact meaning of this depends on the type,
    but is generally the number of characters used for all, or part, of the
    string. A value of -1 indicates that the default for the type should be
    used.
    * width. The minimum number of characters that will be in the produced
    string. If necessary the string will be padded with the fill character to
    make it long enough.
    *align. Specify whether fill characters should be added at the beginning or
    end of the generated string, or both.
    *fill: The character to pad a string with if is is shorter than width.
    """

primitive FormatDefault
primitive PrefixDefault
primitive AlignLeft
primitive AlignRight
primitive AlignCenter

type Align is
  ( AlignLeft
  | AlignRight
  | AlignCenter)

primitive FormatUTF32
primitive FormatBinary
primitive FormatBinaryBare
primitive FormatOctal
primitive FormatOctalBare
primitive FormatHex
primitive FormatHexBare
primitive FormatHexSmall
primitive FormatHexSmallBare

type FormatInt is
  ( FormatDefault
  | FormatUTF32
  | FormatBinary
  | FormatBinaryBare
  | FormatOctal
  | FormatOctalBare
  | FormatHex
  | FormatHexBare
  | FormatHexSmall
  | FormatHexSmallBare)

primitive PrefixSpace
primitive PrefixSign

type PrefixNumber is
  ( PrefixDefault
  | PrefixSpace
  | PrefixSign)

primitive FormatExp
primitive FormatExpLarge
primitive FormatFix
primitive FormatFixLarge
primitive FormatGeneral
primitive FormatGeneralLarge

type FormatFloat is
  ( FormatDefault
  | FormatExp
  | FormatExpLarge
  | FormatFix
  | FormatFixLarge
  | FormatGeneral
  | FormatGeneralLarge)

primitive _ToString
  fun _large(): String => "0123456789ABCDEF"
  fun _small(): String => "0123456789abcdef"

  fun _fmt_int(fmt: FormatInt): (U64, String, String) =>
    match fmt
    | FormatBinary => (2, "b0", _large())
    | FormatBinaryBare => (2, "", _large())
    | FormatOctal => (8, "o0", _large())
    | FormatOctalBare => (8, "", _large())
    | FormatHex => (16, "x0", _large())
    | FormatHexBare => (16, "", _large())
    | FormatHexSmall => (16, "x0", _small())
    | FormatHexSmallBare => (16, "", _small())
    else
      (10, "", _large())
    end

  fun _prefix(neg: Bool, prefix: PrefixNumber): String =>
    if neg then
      "-"
    else
      match prefix
      | PrefixSpace => " "
      | PrefixSign => "+"
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

  fun _u64(x: U64, neg: Bool, fmt: FormatInt, prefix: PrefixNumber,
    prec: U64, width: U64, align: Align, fill: U32): String iso^
  =>
    match fmt
    | FormatUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base: U64, var typestring: String, var table: String) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)
    var prec' = if prec == -1 then 0 else prec end

    recover
      var s = String((prec' + 1).max(width.max(31)))
      var value = x

      try
        if value == 0 then
          s.push(table(0))
        else
          while value != 0 do
            let index = ((value = value / base) - (value * base))
            s.push(table(index))
          end
        end
      end

      s.append(typestring)
      _extend_digits(s, prec')
      s.append(prestring)
      _pad(s, width, align, fill)
      s
    end

  fun _u128(x: U128, neg: Bool, fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    match fmt
    | FormatUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base': U64, var typestring: String, var table: String) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)
    var base = base'.u128()
    var prec' = if prec == -1 then 0 else prec end

    recover
      var s = String((prec' + 1).max(width.max(31)))
      var value = x

      try
        if value == 0 then
          s.push(table(0))
        else
          while value != 0 do
            let index = ((value = value / base) - (value * base)).u64()
            s.push(table(index))
          end
        end
      end

      s.append(typestring)
      _extend_digits(s, prec')
      s.append(prestring)
      _pad(s, width, align, fill)
      s
    end

  fun _f64(x: F64, fmt: FormatFloat = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: U64 = 6, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    // TODO: prefix, align, fill
    var prec' = if prec == -1 then 6 else prec end

    recover
      var s = String((prec' + 8).max(width.max(31)))
      var f = String(31).append("%")

      if width > 0 then f.append(width.string()) end
      f.append(".").append(prec'.string())

      match fmt
      | FormatExp => f.append("e")
      | FormatExpLarge => f.append("E")
      | FormatFix => f.append("f")
      | FormatFixLarge => f.append("F")
      | FormatGeneral => f.append("g")
      | FormatGeneralLarge => f.append("G")
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
