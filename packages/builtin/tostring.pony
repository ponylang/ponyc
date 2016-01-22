trait val FormatSpec
trait val PrefixSpec

interface Creatable
  new box create()

interface box FormatSettings[F: FormatSpec val = FormatDefault,
  P: PrefixSpec val = PrefixDefault]
  """
  Provide the settings to be used when converting objects to strings.

  The `format` and `prefix` settings are effectively enums. The `FormatDefault`
  and `PrefixDefault` values are acceptable for all Stringable objects. Other
  possible values can be added for each Stringable, without those values having
  to be handled by all Stringables. For example, integers have the option
  `FormatHex`, whereas floats do not.

  * format. Format to use.
  * prefix. Prefix to use.
  * precision. Precision to use. The exact meaning of this depends on the type,
  but is generally the number of characters used for all, or part, of the
  string. A value of -1 indicates that the default for the type should be
  used.
  * width. The minimum number of characters that will be in the produced
  string. If necessary the string will be padded with the fill character to
  make it long enough.
  * align. Whether fill characters should be added at the beginning or end of
  the generated string, or both.
  * fill: The character to pad a string with if it is shorter than width.
  """
  fun format(): (F | FormatDefault)
  fun prefix(): (P | PrefixDefault)
  fun precision(): USize
  fun width(): USize
  fun align(): Align
  fun fill(): U32

primitive FormatSettingsDefault
  """
  Default format settings for general Stringable objects.
  """
  fun format(): FormatDefault => FormatDefault
  fun prefix(): PrefixDefault => PrefixDefault
  fun precision(): USize => -1
  fun width(): USize => 0
  fun align(): Align => AlignLeft
  fun fill(): U32 => ' '

class FormatSettingsHolder[F: FormatSpec val = FormatDefault,
  P: PrefixSpec val = PrefixDefault,
  D: (FormatSettings[F, P] #read & Creatable #read) = FormatSettingsDefault]
  """
  Modifiable format settings holder.

  Each setting should be set as appropriate and then the holder can be passed
  to the relevant `string()` functions. The settings can then be tweaked and
  the holder reused for other `string()` calls if required.

  Default settings may be provided to the `create()` constructor. If they
  aren't, defaults are specified by an instance of the `D` type parameter.

  Settings may be read and written directly or the provided accessors may be
  used. The write accessors return `this` to allow call chaining.
  """
  var format': (F | FormatDefault)
  var prefix': (P | PrefixDefault)
  var precision': USize
  var width': USize
  var align': Align
  var fill': U32

  new create(defaults: FormatSettings[F, P] = D) =>
    """
    Initialise fields using the given default source.
    """
    format' = defaults.format()
    prefix' = defaults.prefix()
    precision' = defaults.precision()
    width' = defaults.width()
    align' = defaults.align()
    fill' = defaults.fill()

  fun format(): (F | FormatDefault) => format'
  fun prefix(): (P | PrefixDefault) => prefix'
  fun precision(): USize => precision'
  fun width(): USize => width'
  fun align(): Align => align'
  fun fill(): U32 => fill'

  fun ref set_format(f: (F | FormatDefault)): FormatSettingsHolder[F, P, D] =>
    format' = f
    this

  fun ref set_prefix(p: (P | PrefixDefault)): FormatSettingsHolder[F, P, D] =>
    prefix' = p
    this

  fun ref set_precision(p: USize): FormatSettingsHolder[F, P, D] =>
    precision' = p
    this

  fun ref set_width(w: USize): FormatSettingsHolder[F, P, D] =>
    width' = w
    this

  fun ref set_align(a: Align): FormatSettingsHolder[F, P, D] =>
    align' = a
    this

  fun ref set_fill(f: U32): FormatSettingsHolder[F, P, D] =>
    fill' = f
    this

interface box Stringable[F: FormatSpec val = FormatDefault,
  P: PrefixSpec val = PrefixDefault]
  """
  Things that can be turned into a String.
  """
  fun string(fmt: FormatSettings[F, P] = FormatSettingsDefault): String iso^
    """
    Generate a string representation of this object.

    Formatting information may be specified by providing a `FormatSettings`
    object. If no object is explicitly provided a default is used.

    When only fixed values are needed a primitive can be used, which means no
    storage allocation is needed.
    """

primitive FormatDefault is FormatSpec
primitive PrefixDefault is PrefixSpec
primitive AlignLeft
primitive AlignRight
primitive AlignCenter

type Align is
  ( AlignLeft
  | AlignRight
  | AlignCenter)

primitive FormatUTF32 is FormatSpec
primitive FormatBinary is FormatSpec
primitive FormatBinaryBare is FormatSpec
primitive FormatOctal is FormatSpec
primitive FormatOctalBare is FormatSpec
primitive FormatHex is FormatSpec
primitive FormatHexBare is FormatSpec
primitive FormatHexSmall is FormatSpec
primitive FormatHexSmallBare is FormatSpec

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

primitive FormatDefaultNumber
  """
  Default format settings for numbers.
  """
  fun format(): FormatDefault => FormatDefault
  fun prefix(): PrefixDefault => PrefixDefault
  fun precision(): USize => -1
  fun width(): USize => 0
  fun align(): Align => AlignRight
  fun fill(): U32 => ' '

type FormatSettingsInt is
  FormatSettingsHolder[FormatInt, PrefixNumber, FormatDefaultNumber]
  """
  Format holder for integers.
  """

primitive PrefixSpace is PrefixSpec
primitive PrefixSign is PrefixSpec

type PrefixNumber is
  ( PrefixDefault
  | PrefixSpace
  | PrefixSign)

primitive FormatExp is FormatSpec
primitive FormatExpLarge is FormatSpec
primitive FormatFix is FormatSpec
primitive FormatFixLarge is FormatSpec
primitive FormatGeneral is FormatSpec
primitive FormatGeneralLarge is FormatSpec

type FormatFloat is
  ( FormatDefault
  | FormatExp
  | FormatExpLarge
  | FormatFix
  | FormatFixLarge
  | FormatGeneral
  | FormatGeneralLarge)

type FormatSettingsFloat is
  FormatSettingsHolder[FormatFloat, PrefixNumber, FormatDefaultNumber]
  """
  Format holder for floats.
  """


primitive _ToString
  """
  Worker type providing to string conversions for numbers.
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

  fun _u64(x: U64, neg: Bool,
    fmt: FormatSettings[FormatInt, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    match fmt.format()
    | FormatUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base', var typestring, var table) = _fmt_int(fmt.format())
    var prestring = _prefix(neg, fmt.prefix())
    var prec' = if fmt.precision() == -1 then 0 else fmt.precision() end
    let base = base'.u64()
    let width' = fmt.width()
    let align' = fmt.align()
    let fill' = fmt.fill()

    recover
      var s = String((prec' + 1).max(width'.max(31)))
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

      s.append(typestring)
      _extend_digits(s, prec')
      s.append(prestring)
      _pad(s, width', align', fill')
      s
    end

  fun _u128(x: U128, neg: Bool,
    fmt: FormatSettings[FormatInt, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    match fmt.format()
    | FormatUTF32 => return recover String.from_utf32(x.u32()) end
    end

    (var base', var typestring, var table) = _fmt_int(fmt.format())
    var prestring = _prefix(neg, fmt.prefix())
    var base = base'.u128()
    var prec' = if fmt.precision() == -1 then 0 else fmt.precision() end
    let width' = fmt.width()
    let align' = fmt.align()
    let fill' = fmt.fill()

    recover
      var s = String((prec' + 1).max(width'.max(31)))
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

      s.append(typestring)
      _extend_digits(s, prec')
      s.append(prestring)
      _pad(s, width', align', fill')
      s
    end

  fun _f64(x: F64,
    fmt: FormatSettings[FormatFloat, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    // TODO: prefix, align, fill
    let format' = fmt.format()
    var prec' = if fmt.precision() == -1 then 6 else fmt.precision() end
    let width' = fmt.width()

    recover
      var s = String((prec' + 8).max(width'.max(31)))
      var f = String(31).append("%")

      if width' > 0 then f.append(width'.string()) end
      f.append(".").append(prec'.string())

      match format'
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
