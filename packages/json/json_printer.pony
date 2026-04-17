primitive JsonPrinter
  """
  Serialize any `JsonValue` to a JSON string.

  `JsonObject` and `JsonArray` already implement `Stringable`, but the other
  members of the `JsonValue` union — `String`, `I64`, `F64`, `Bool`, and
  `None` — do not produce valid JSON from their `.string()` methods (e.g.,
  `None.string()` yields `"None"`, not `"null"`; strings are not escaped or
  quoted). `JsonPrinter` handles every variant correctly:

  ```pony
  use json = "json"

  // Compact output
  json.JsonPrinter.print("hello \"world\"")  // "hello \"world\""
  json.JsonPrinter.print(None)               // null
  json.JsonPrinter.print(true)               // true

  // Pretty-printed output (objects and arrays)
  let obj = json.JsonObject
    .update("name", "Alice")
    .update("scores", json.JsonArray.push(I64(1)).push(I64(2)))
  json.JsonPrinter.pretty(obj)
  ```
  """

  fun print(value: JsonValue): String iso^ =>
    """Compact JSON serialization of any `JsonValue`."""
    _JsonPrint.compact(value)

  fun pretty(value: JsonValue, indent: String = "  "): String iso^ =>
    """Pretty-printed JSON serialization of any `JsonValue`."""
    _JsonPrint.pretty(value, indent)
