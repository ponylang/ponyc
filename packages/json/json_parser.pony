primitive JSONParser
  """
  High-level JSON parser. Returns a structured result rather than raising.

  ```pony
  match JSONParser.parse(source)
  | let json: JSONValue => // use json
  | let err: JSONParseError => env.err.print(err.string())
  end
  ```

  Built on top of `JSONTokenParser`: the token parser handles all parsing
  logic, and a `JSONReassembler` assembles the tokens into the result.
  `parse()` applies no resource limits — it is for a whole document already
  in memory, which is trusted to be as large as it is. For input of unknown
  origin, or that arrives in pieces, use `JSONTokenParser` directly with a
  `JSONParseLimits`.
  """

  fun parse(source: String): (JSONValue | JSONParseError) =>
    """
    Parse a complete JSON document from a string.
    """
    // A whole in-memory document is trusted, so parse it with no limits.
    let notify = _FirstValue
    let parser = JSONTokenParser(notify, JSONParseLimits.unlimited())
    try
      parser.feed(source)?
      parser.finish()?
    else
      if notify.trailing then
        return JSONParseError(
          "Trailing content after JSON value",
          parser.token_end(),
          parser.line())
      end
      return parser.parse_error()
    end

    let vs = notify.take_values()
    if vs.size() == 0 then
      if parser.incomplete() then
        JSONParseError(
          "Unexpected end of input",
          parser.token_end(),
          parser.line())
      else
        JSONParseError("Empty input", 0, 1)
      end
    else
      try vs(0)? else _Unreachable(); JSONParseError("Empty input", 0, 1) end
    end

class ref _FirstValue is JSONTokenNotify
  """
  `JSONParser`'s notifier. It assembles exactly the first top-level value;
  the moment a second top-level value begins it aborts the parse, so
  trailing content is detected without being built — matching the batch
  parser's contract that a document is a single value and any following
  content is an error.
  """
  embed _reassembler: JSONReassembler
  var _depth: USize = 0
  var _done: Bool = false
  var trailing: Bool = false
    """Set when a second top-level value began, aborting the parse."""

  new ref create() =>
    _reassembler = JSONReassembler

  fun ref apply(parser: JSONTokenParser, token: JSONToken) =>
    if _done then
      // A second top-level value is starting; stop before building any of it.
      trailing = true
      parser.abort()
      return
    end
    _reassembler.add(token)
    match token
    | JSONTokenObjectStart | JSONTokenArrayStart => _depth = _depth + 1
    | JSONTokenObjectEnd | JSONTokenArrayEnd =>
      _depth = _depth - 1
      if _depth == 0 then _done = true end
    | let k: JSONTokenKey => None // a key is always inside an object
    else
      // a scalar value; at the top level it completes the document
      if _depth == 0 then _done = true end
    end

  fun ref take_values(): Array[JSONValue] iso^ =>
    _reassembler.take_values()
