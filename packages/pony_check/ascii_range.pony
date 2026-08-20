
primitive ASCIINUL
  """
  The NUL character.
  """
  fun apply(): String => "\x00"

primitive ASCIIDigits
  """
  The ASCII digit characters 0-9.
  """
  fun apply(): String => "0123456789"

primitive ASCIIWhiteSpace
  """
  The ASCII whitespace characters.
  """
  fun apply(): String => " \t\n\r\x0b\x0c"

primitive ASCIIPunctuation
  """
  The ASCII punctuation characters.
  """
  fun apply(): String => "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"

primitive ASCIILettersLower
  """
  The lowercase ASCII letter characters a-z.
  """
  fun apply(): String => "abcdefghijklmnopqrstuvwxyz"

primitive ASCIILettersUpper
  """
  The uppercase ASCII letter characters A-Z.
  """
  fun apply(): String => "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

primitive ASCIILetters
  """
  All ASCII letter characters, both lowercase and uppercase.
  """
  fun apply(): String => ASCIILettersLower() + ASCIILettersUpper()

primitive ASCIIPrintable
  """
  All printable ASCII characters.
  """
  fun apply(): String =>
    ASCIIDigits() +
      ASCIILetters() +
      ASCIIPunctuation() +
      ASCIIWhiteSpace()

primitive ASCIINonPrintable
  """
  The non-printable ASCII characters, excluding NUL.
  """
  fun apply(): String =>
    "\x01\x02\x03\x04\x05\x06\x07\x08\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"

primitive ASCIIAll
  """
  Represents all ASCII characters,
  excluding the NUL (\x00) character for its special treatment in C strings.
  """
  fun apply(): String =>
    ASCIIPrintable() + ASCIINonPrintable()

primitive ASCIIAllWithNUL
  """
  Represents all ASCII characters,
  including the NUL (\x00) character for its special treatment in C strings.
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
    | ASCIIAllWithNUL
    )
