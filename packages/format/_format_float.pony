primitive _FormatFloat
  """
  Worker type providing to string conversions for floats.
  """
  fun f64(x: F64, fmt: FormatFloat = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: USize = 6, width: USize = 0,
    align: Align = AlignRight, fill: U32 = ' '
  ): String iso^ =>
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

      ifdef windows then
        @_snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      else
        @snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      end

      s.recalc()
    end
