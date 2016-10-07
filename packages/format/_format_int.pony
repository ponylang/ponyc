primitive _FormatInt
  """
  Worker type providing to string conversions for integers.
  """
  fun _large(): String => "0123456789ABCDEF"
  fun _small(): String => "0123456789abcdef"

  fun _fmt_int(fmt: FormatInt): (U32, String, String) =>
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

  fun _extend_digits(s: String ref, digits: USize) =>
    while s.size() < digits do
      s.append("0")
    end

  fun _pad(s: String ref, width: USize, align: Align, fill: U32) =>
    var pre: USize = 0
    var post: USize = 0

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

  fun u64(x: U64, neg: Bool, fmt: FormatInt, prefix: PrefixNumber,
    prec: USize, width: USize, align: Align, fill: U32
  ): String iso^ =>
    match fmt
    | FormatUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base', var typestring, var table) = _fmt_int(fmt)
    var prestring = _prefix(neg, prefix)
    var prec' = if prec == -1 then 0 else prec end
    let base = base'.u64()

    recover
      var s = String((prec + 1).max(width.max(31)))
      var value = x

      try
        if value == 0 then
          s.push(table(0))
        else
          while value != 0 do
            let index = ((value = value / base) - (value * base))
            s.push(table(index.usize()))
          end
        end
      end

      _extend_digits(s, prec')
      s.append(typestring)
      s.append(prestring)
      _pad(s, width, align, fill)
      s
    end

  fun u128(x: U128, neg: Bool, fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: USize = -1, width: USize = 0,
    align: Align = AlignLeft, fill: U32 = ' '
  ): String iso^ =>
    match fmt
    | FormatUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base', var typestring, var table) = _fmt_int(fmt)
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
            let index = (value = value / base) - (value * base)
            s.push(table(index.usize()))
          end
        end
      end

      _extend_digits(s, prec')
      s.append(typestring)
      s.append(prestring)
      _pad(s, width, align, fill)
      s
    end
