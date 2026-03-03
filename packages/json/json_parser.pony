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
