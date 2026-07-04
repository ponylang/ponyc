primitive JsonParser
  """
  High-level JSON parser. Returns a structured result rather than raising.

  ```pony
  match JsonParser.parse(source)
  | let json: JsonValue => // use json
  | let err: JsonParseError => env.err.print(err.string())
  end
  ```

  Built on top of JsonTokenParser — the token parser handles all parsing
  logic, and an internal tree builder assembles the result.

  A numeric literal outside `F64` range — for example `1e999`, or an integer of
  several hundred digits — is rejected as a parse error rather than parsed to a
  non-finite value. RFC 8259 permits an implementation to limit numeric range.
  """

  fun parse(source: String): (JsonValue | JsonParseError) =>
    """Parse a complete JSON document from a string."""
    let builder = _TreeBuilder
    let tokenizer = JsonTokenParser(builder)
    try
      tokenizer.parse(source)?
    else
      return JsonParseError(
        tokenizer.describe_error(),
        tokenizer.token_end(),
        tokenizer.line())
    end
    match \exhaustive\ builder.result()
    | let json: JsonValue => json
    | _NoResult => JsonParseError("Empty input", 0, 1)
    end
