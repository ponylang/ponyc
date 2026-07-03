interface JsonTokenNotify
  """
  Callback interface for `JsonTokenParser`.

  Implement this to process JSON tokens as they are parsed, without
  materializing the full document tree. Read a token's value off the token
  itself (`JsonTokenKey.value`, `JsonTokenString.value`, `JsonTokenNumber.value`);
  the structural and literal tokens are primitives that carry no data.

  Token emission contract: objects emit ObjectStart, then alternating
  Key/value sequences, then ObjectEnd. Arrays emit ArrayStart, then
  values, then ArrayEnd. `JsonReassembler` relies on this order; changing it
  would break any such consumer.
  """

  fun ref apply(parser: JsonTokenParser, token: JsonToken)
    """
    Called for each token parsed from the JSON document. `token` carries its own
    value. Use `parser` to read the token's byte position (`token_start()`,
    `token_end()`, `line()`) and to call `abort()` if parsing should stop early;
    those positions are valid only during this call.
    """
