interface JsonTokenNotify
  """
  Callback interface for the streaming JSON token parser.

  Implement this to process JSON tokens as they are parsed, without
  materializing the full document tree.

  Token emission contract: objects emit ObjectStart, then alternating
  Key/value sequences, then ObjectEnd. Arrays emit ArrayStart, then
  values, then ArrayEnd. Changes to this emission order would break
  consumers — this contract is documented here at the point of coupling.
  """

  fun ref apply(parser: JsonTokenParser, token: JsonToken)
    """
    Called for each token parsed from the JSON document. Use `parser` to
    access parsed values (`last_string`, `last_number`) and to call
    `abort()` if parsing should stop early.
    """
