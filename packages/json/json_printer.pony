primitive JsonPrinter
  """
  Serialize any `JsonValue` to a JSON string. This is the dual of `JsonParser`:
  where `JsonParser.parse` turns a `String` into a `JsonValue`, `JsonPrinter`
  turns a `JsonValue` back into a JSON `String`.

  To serialize Pony data as JSON, build a `JsonValue` and pass it to `print`:

  ```pony
  let doc = JsonObject
    .update("name", "Alice")
    .update("age", I64(30))

  env.out.print(JsonPrinter.print(doc))
  // {"name":"Alice","age":30}
  ```

  `print` produces compact output; `pretty` produces indented output. Both
  accept any `JsonValue`, including scalars (`String`, `I64`, `F64`, `Bool`)
  and JSON null (`None`):

  ```pony
  JsonPrinter.print(None)   // null
  JsonPrinter.print(true)   // true
  JsonPrinter.print("hi")   // "hi"
  ```

  A non-finite `F64` — infinity or NaN — has no JSON representation (RFC 8259
  numbers are finite), so it serializes as `null`.
  """

  fun print(value: JsonValue): String iso^ =>
    """Compact JSON serialization of any `JsonValue`."""
    _JsonPrint.compact(value)

  fun pretty(value: JsonValue, indent: String = "  "): String iso^ =>
    """Pretty-printed JSON serialization of any `JsonValue`."""
    _JsonPrint.pretty(value, indent)
