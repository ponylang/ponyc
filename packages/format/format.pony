use "collections"

primitive Format
  """
  Provides functions for generating formatted strings.

  * fmt. Format to use.
  * prefix. Prefix to use.
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
  fun apply(str: String, fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: USize = -1, width: USize = 0,
    align: Align = AlignLeft, fill: U32 = ' '
  ): String iso^ =>
    let copy_len = str.size().min(prec.usize())
    let len = copy_len.max(width.usize())
    recover
      let s = String(len)

      match align
      | AlignLeft =>
        s.append(str)
        for i in Range(s.size(), s.space()) do
          s.push_utf32(fill)
        end
      | AlignRight =>
        for i in Range(0, len - copy_len) do
          s.push_utf32(fill)
        end
        s.append(str)
      | AlignCenter =>
        let half = (len - copy_len) / 2
        for i in Range(0, half) do
          s.push_utf32(fill)
        end
        s.append(str)
        for i in Range(s.size(), s.space()) do
          s.push_utf32(fill)
        end
      end

      s.recalc()
    end

  fun int[A: (Int & Integer[A])](x: A, fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: USize = -1, width: USize = 0,
    align: Align = AlignRight, fill: U32 = ' '
  ): String iso^ =>
    let zero = x.from[USize](0)
    (let abs, let neg) = if x < zero then (-x, true) else (x, false) end

    if x is U128 then
      _FormatInt.u128(x.u128(), false, fmt, prefix, prec, width, align, fill)
    elseif x is I128 then
      _FormatInt.u128(abs.u128(), neg, fmt, prefix, prec, width, align, fill)
    else
      _FormatInt.u64(abs.u64(), neg, fmt, prefix, prec, width, align, fill)
    end

  fun float[A: (Float & FloatingPoint[A])](x: A,
    fmt: FormatFloat = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: USize = 6, width: USize = 0,
    align: Align = AlignRight, fill: U32 = ' '
  ): String iso^ =>
    _FormatFloat.f64(x.f64(), fmt, prefix, prec, width, align, fill)
