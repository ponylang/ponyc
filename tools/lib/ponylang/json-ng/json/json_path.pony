class val JsonPath
  """
  Compiled JSONPath query for extracting values from JSON documents.

  JSONPath queries navigate JSON structures using string path expressions
  (RFC 9535). Compile a path with `JsonPathParser.parse()`, then apply
  it to any number of documents with `query()`:

  ```pony
  match JsonPathParser.parse("$.store.book[*].author")
  | let path: JsonPath =>
    let authors = path.query(doc)
  | let err: JsonPathParseError =>
    env.err.print(err.string())
  end
  ```

  Evaluation follows RFC 9535 semantics: missing keys, out-of-bounds
  indices, and type mismatches produce empty results, never errors.
  Only malformed path strings produce errors (at parse time). Filter
  expressions support function extensions (`length`, `count`, `match`,
  `search`, `value`) per RFC 9535 Section 2.4.

  For simple single-value extraction, `query_one()` returns the first
  match or JsonNotFound.
  """

  let _segments: Array[_Segment] val

  new val _create(segments': Array[_Segment] val) =>
    _segments = segments'

  fun query(root: JsonValue): Array[JsonValue] val =>
    """
    Execute this query against a JSON document.

    Returns all matching values. Returns an empty array if no values
    match. Evaluation never errors.
    """
    _JsonPathEval(root, root, _segments)

  fun query_one(root: JsonValue): (JsonValue | JsonNotFound) =>
    """
    Execute this query and return the first matching value, or
    JsonNotFound if no values match.

    Convenience for paths known to select at most one value.
    """
    let results = query(root)
    if results.size() > 0 then
      try results(0)? else JsonNotFound end
    else
      JsonNotFound
    end


primitive JsonPathParser
  """
  Parser for JSONPath expressions.

  Provides two entry points:
  - `parse()` returns errors as data (consistent with `JsonParser.parse()`)
  - `compile()` raises on invalid input (convenience for known-valid paths)
  """

  fun parse(path: String): (JsonPath | JsonPathParseError) =>
    """
    Parse a JSONPath expression. Returns a compiled query on success
    or a structured error on failure.
    """
    let parser = _JsonPathParser(path)
    try
      let segments = parser.parse()?
      JsonPath._create(segments)
    else
      parser.error_result()
    end

  fun compile(path: String): JsonPath ? =>
    """
    Parse a JSONPath expression, raising on invalid input.

    Use this when the path string is known to be valid (e.g., a string
    literal). For user-provided paths, prefer `parse()` which returns
    errors as data.
    """
    match JsonPathParser.parse(path)
    | let jp: JsonPath => jp
    | let _: JsonPathParseError => error
    end


class val JsonPathParseError is Stringable
  """Structured parse error for JSONPath expressions."""

  let message: String
  let offset: USize

  new val create(message': String, offset': USize) =>
    message = message'
    offset = offset'

  fun string(): String iso^ =>
    let s = recover iso String(64) end
    s.append("JSONPath parse error at offset ")
    s.append(offset.string())
    s.append(": ")
    s.append(message)
    consume s
