trait val FormatSpec
  """
  Base trait for all format specifiers.
  """

primitive FormatDefault is FormatSpec
  """
  Use the default format for the type being formatted.
  """

primitive FormatUTF32 is FormatSpec
  """
  Format an integer as a UTF-32 character.
  """

primitive FormatBinary is FormatSpec
  """
  Format an integer in binary with a leading `0b` prefix.
  """

primitive FormatBinaryBare is FormatSpec
  """
  Format an integer in binary without a prefix.
  """

primitive FormatOctal is FormatSpec
  """
  Format an integer in octal with a leading `0o` prefix.
  """

primitive FormatOctalBare is FormatSpec
  """
  Format an integer in octal without a prefix.
  """

primitive FormatHex is FormatSpec
  """
  Format an integer in uppercase hexadecimal with a leading `0x` prefix.
  """

primitive FormatHexBare is FormatSpec
  """
  Format an integer in uppercase hexadecimal without a prefix.
  """

primitive FormatHexSmall is FormatSpec
  """
  Format an integer in lowercase hexadecimal with a leading `0x` prefix.
  """

primitive FormatHexSmallBare is FormatSpec
  """
  Format an integer in lowercase hexadecimal without a prefix.
  """

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
  | FormatHexSmallBare )

primitive FormatExp is FormatSpec
  """
  Format a float in lowercase scientific notation.
  """

primitive FormatExpLarge is FormatSpec
  """
  Format a float in uppercase scientific notation.
  """

primitive FormatFix is FormatSpec
  """
  Format a float in lowercase fixed-point notation.
  """

primitive FormatFixLarge is FormatSpec
  """
  Format a float in uppercase fixed-point notation.
  """

primitive FormatGeneral is FormatSpec
  """
  Format a float in lowercase general notation.
  """

primitive FormatGeneralLarge is FormatSpec
  """
  Format a float in uppercase general notation.
  """

type FormatFloat is
  ( FormatDefault
  | FormatExp
  | FormatExpLarge
  | FormatFix
  | FormatFixLarge
  | FormatGeneral
  | FormatGeneralLarge )
