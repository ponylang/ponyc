use json = "json"

primitive ResponseJSON
  """
  Parse the body of an `HTTPResponse` as JSON.

  Returns the parsed `JSONValue` on success, or `JSONParseError` if the body
  is not valid JSON. This is deliberately minimal — users then use
  `JSONNav`, `JSONLens`, pattern matching, or whatever access pattern
  they prefer.

  ```pony
  use json = "json"

  match ResponseJSON(response)
  | let value: json.JSONValue =>
    // work with the parsed JSON
  | let err: json.JSONParseError =>
    env.out.print("Parse error: " + err.string())
  end
  ```
  """

  fun apply(response: HTTPResponse): (json.JSONValue | json.JSONParseError) =>
    """
    Parse `response.body` as JSON.

    Converts the body bytes to a `String` and parses with `JSONParser.parse()`.
    Returns `JSONParseError` if the body is empty or contains invalid JSON.
    """
    let body_str = String.from_array(response.body)
    json.JSONParser.parse(body_str)
