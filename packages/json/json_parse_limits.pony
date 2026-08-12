class val JsonParseLimits
  """
  Resource bounds for `JsonTokenParser` over untrusted input.

  These bound the parser's own working memory: the container-depth stack, and the
  single string or number it is part-way through. They do not bound how much a
  `JsonReassembler` — or your own notifier — chooses to retain; that memory is the
  caller's choice, so a raw-notifier consumer that drops the tokens it doesn't need
  stays flat no matter how large the document.

  The default constructor applies conservative limits; raise a specific bound when
  a trusted source legitimately has a large value or deep nesting, or use
  `unlimited()` for fully trusted input.

  ```pony
  let parser = JsonTokenParser(notify) // default limits
  let parser = JsonTokenParser(notify, JsonParseLimits(where max_depth' = 32))
  let parser = JsonTokenParser(notify, JsonParseLimits.unlimited())
  ```
  """
  let max_depth: USize
    """Maximum container nesting depth. This bounds the parser's frame stack."""

  let max_string_len: USize
    """
    Maximum length, in bytes, of a single string's source text (the bytes between
    the quotes, before any escapes are decoded).
    """

  let max_number_len: USize
    """Maximum length, in bytes, of a single number's source text."""

  new val create(
    max_depth': USize = 1024,
    max_string_len': USize = 1_048_576,
    max_number_len': USize = 256)
  =>
    max_depth = max_depth'
    max_string_len = max_string_len'
    max_number_len = max_number_len'

  new val unlimited() =>
    """
    No bounds — every limit is set to its maximum. For fully trusted input, such
    as a whole document already held in memory. This is what `JsonParser.parse`
    uses.
    """
    max_depth = USize.max_value()
    max_string_len = USize.max_value()
    max_number_len = USize.max_value()
