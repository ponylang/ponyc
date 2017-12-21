trait val FormatSpec

primitive FormatDefault is FormatSpec

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
  | FormatHexSmallBare )

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
  | FormatGeneralLarge )
