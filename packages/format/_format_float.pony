use @snprintf[I32](str: Pointer[U8] tag, size: USize, fmt: Pointer[U8] tag, ...)
  if not windows
use @_snprintf[I32](
  str: Pointer[U8] tag, count: USize, fmt: Pointer[U8] tag, ...)
  if windows
use @_scprintf[I32](fmt: Pointer[U8] tag, ...) if windows

primitive _FormatFloat
  """
  Worker type providing to string conversions for floats.
  """
  fun f64(
    x: F64,
    fmt: FormatFloat = FormatDefault,
    prefix: PrefixNumber = PrefixDefault,
    prec: USize = 6,
    width: USize = 0,
    align: Align = AlignRight,
    fill: U32 = ' ')
    : String iso^
  =>
    // TODO: prefix, align, fill
    var prec' = if prec == -1 then 6 else prec end

    recover
      var f = String(31) .> append("%")

      if width > 0 then f.append(width.string()) end
      f .> append(".") .> append(prec'.string())

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

      // Ask snprintf how many characters the formatted value needs, then
      // allocate a buffer large enough. On POSIX, snprintf(NULL, 0, ...)
      // returns the required length; on Windows, _scprintf does the same.
      let needed: USize =
        ifdef windows then
          @_scprintf(f.cstring(), x).usize()
        else
          @snprintf(Pointer[U8], USize(0), f.cstring(), x).usize()
        end

      var s = String((needed + 1).max(width.max(31)))

      ifdef windows then
        @_snprintf(s.cstring(), s.space(), f.cstring(), x)
      else
        @snprintf(s.cstring(), s.space(), f.cstring(), x)
      end

      s .> recalc()
    end
