trait val FormatSpec
trait val PrefixSpec

interface Creatable
  new box create()

interface box FormatSettings[F: FormatSpec val = FormatDefault,
  P: PrefixSpec val = PrefixDefault]
  """
  Settings used when converting objects to strings.
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
