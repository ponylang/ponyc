class val JSONDecodeError is Stringable
  """
  A structural mismatch when decoding parsed JSON into a domain type.

  `JSONDecodeError` is distinct from `JSONParseError`:
  `JSONParseError` means the response body was not valid JSON syntax, while
  `JSONDecodeError` means the JSON was syntactically valid but its structure
  doesn't match what the decoder expected — a missing field, a field with the
  wrong type, or any other shape mismatch. The message should describe what was
  expected versus what was found.
  """
  let message: String

  new val create(message': String) =>
    """
    Create a decode error with a descriptive message.
    """
    message = message'

  fun string(): String iso^ =>
    """
    Return the error message.
    """
    message.clone()
