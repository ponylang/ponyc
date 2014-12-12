primitive IntUTF32
primitive IntBinary
primitive IntOctal
primitive IntHex
primitive IntHexSmall
primitive IntNoPrefix
primitive IntSignPrefix

type IntFormat is
  ( FormatDefault
  | IntUTF32
  | IntBinary
  | IntOctal
  | IntHex
  | IntHexSmall)

type IntPrefix is
  ( PrefixDefault
  | IntNoPrefix
  | IntSignPrefix)

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
  fun tag _large(): String => "FEDCBA9876543210123456789ABCDEF"
  fun tag _small(): String => "fedcba9876543210123456789abcdef"

  fun box _u64(x: U64, neg: Bool, fmt: IntFormat, prefix: IntPrefix, prec: U64,
    width: U64, align: Align, fill: U8): String iso^
  =>
    // TODO: prefix
    var table = _large()

    var base = match fmt
    | IntUTF32 => return recover String.append_utf32(x.u32()) end
    | IntBinary => U64(2)
    | IntOctal => U64(8)
    | IntHex => U64(16)
    | IntHexSmall => table = _small(); U64(16)
    else
      U64(10)
    end

    recover
      var s = String.reserve((prec + 1).max(width.max(31)))
      var value = x
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / base
          let index = tmp - (value * base)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end

        while i < prec.i64() do
          s.append_byte('0')
          i = i + 1
        end

        if neg then
          s.append_byte('-')
          i = i + 1
        end

        var pre: U64 = 0
        var post: U64 = 0

        if i.u64() < width then
          let rem = width - i.u64()

          match align
          | AlignLeft => post = rem
          | AlignRight => pre = rem
          | AlignCenter => pre = rem / 2; post = rem - pre
          end
        end

        while pre > 0 do
          s.append_byte(fill)
          pre = pre - 1
        end

        s.reverse_in_place()

        while post > 0 do
          s.append_byte(fill)
          post = post - 1
        end
      end

      consume s
    end

  fun box _u128(x: U128, neg: Bool, fmt: IntFormat = FormatDefault,
    prefix: IntPrefix = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U8 = ' '): String iso^
  =>
    // TODO: prefix
    var table = _large()

    var base = match fmt
    | IntUTF32 => return recover String.append_utf32(x.u32()) end
    | IntBinary => U128(2)
    | IntOctal => U128(8)
    | IntHex => U128(16)
    | IntHexSmall => table = _small(); U128(16)
    else
      U128(10)
    end

    recover
      var s = String.reserve((prec + 1).max(width.max(31)))
      var value = x
      var i: I64 = 0

      try
        while value != 0 do
          let tmp = value
          value = value / base
          let index = tmp - (value * base)
          s.append_byte(table((index + 15).i64()))
          i = i + 1
        end

        while i < prec.i64() do
          s.append_byte('0')
          i = i + 1
        end

        if neg then
          s.append_byte('-')
          i = i + 1
        end

        var pre: U64 = 0
        var post: U64 = 0

        if i.u64() < width then
          let rem = width - i.u64()

          match align
          | AlignLeft => post = rem
          | AlignRight => pre = rem
          | AlignCenter => pre = rem / 2; post = rem - pre
          end
        end

        while pre > 0 do
          s.append_byte(fill)
          pre = pre - 1
        end

        s.reverse_in_place()

        while post > 0 do
          s.append_byte(fill)
          post = post - 1
        end
      end

      consume s
    end

  fun box _f64(x: F64, fmt: FloatFormat = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = 6, width: U64 = 0,
    align: Align = AlignRight, fill: U8 = ' '): String iso^
  =>
    // TODO: prefix, align, fill
    recover
      var s = String.prealloc((prec + 8).max(width.max(31)))
      var f = String.reserve(31).append("%")

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
