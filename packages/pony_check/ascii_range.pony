
primitive ASCIINUL
  fun apply(): String => "\x00"

primitive ASCIIDigits
  fun apply(): String => "0123456789"

primitive ASCIIWhiteSpace
  fun apply(): String => " \t\n\r\x0b\x0c"

primitive ASCIIPunctuation
  fun apply(): String => "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"

primitive ASCIILettersLower
  fun apply(): String => "abcdefghijklmnopqrstuvwxyz"

primitive ASCIILettersUpper
  fun apply(): String => "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

primitive ASCIILetters
  fun apply(): String => ASCIILettersLower() + ASCIILettersUpper()

primitive ASCIIPrintable
  fun apply(): String =>
    ASCIIDigits()
      + ASCIILetters()
      + ASCIIPunctuation()
      + ASCIIWhiteSpace()

primitive ASCIINonPrintable
  fun apply(): String =>
    "\x01\x02\x03\x04\x05\x06\x07\x08\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"

primitive ASCIIAll
  """
  represents all ASCII characters
  excluding the NUL (\x00) character for its special treatment in C strings
  """
  fun apply(): String =>
    ASCIIPrintable() + ASCIINonPrintable()

primitive ASCIIAllWithNUL
  """
  represents all ASCII characters
  including the NUL (\x00) character for its special treatment in C strings
  """
  fun apply(): String =>
    ASCIIAll() + ASCIINUL()

type ASCIIRange is
    ( ASCIINUL
    | ASCIIDigits
    | ASCIIWhiteSpace
    | ASCIIPunctuation
    | ASCIILettersLower
    | ASCIILettersUpper
    | ASCIILetters
    | ASCIIPrintable
    | ASCIINonPrintable
    | ASCIIAll
    | ASCIIAllWithNUL)
