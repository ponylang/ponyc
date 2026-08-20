primitive JSONPrinter
  """
  Serialize any `JSONValue` to a JSON string. This is the dual of `JSONParser`:
  where `JSONParser.parse` turns a `String` into a `JSONValue`, `JSONPrinter`
  turns a `JSONValue` back into a JSON `String`.

  To serialize Pony data as JSON, build a `JSONValue` and pass it to `print`:

  ```pony
  let doc = JSONObject
    .update("name", "Alice")
    .update("age", I64(30))

  env.out.print(JSONPrinter.print(doc))
  // {"name":"Alice","age":30}
  ```

  `print` produces compact output; `pretty` produces indented output. Both
  accept any `JSONValue`, including scalars (`String`, `I64`, `F64`, `Bool`)
  and JSON null (`None`):

  ```pony
  JSONPrinter.print(None)   // null
  JSONPrinter.print(true)   // true
  JSONPrinter.print("hi")   // "hi"
  ```

  A non-finite `F64` — infinity or NaN — has no JSON representation (RFC 8259
  numbers are finite), so it serializes as `null`.
  """

  fun print(value: JSONValue): String iso^ =>
    """
    Compact JSON serialization of any `JSONValue`.
    """
    _JSONPrint.compact(value)

  fun pretty(value: JSONValue, indent: String = "  "): String iso^ =>
    """
    Pretty-printed JSON serialization of any `JSONValue`.
    """
    _JSONPrint.pretty(value, indent)
