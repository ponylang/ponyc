use col = "collections"
use pc = "collections/persistent"

class val JsonObject is Stringable
  """
  Immutable JSON object backed by a persistent hash map.

  Construction is via chained `.update()` calls, each returning a new object
  with structural sharing:

  ```pony
  let obj = JsonObject
    .update("name", "Alice")
    .update("age", I64(30))
  ```
  """

  let _data: pc.Map[String, JsonValue]

  new val create(
    data': pc.Map[String, JsonValue] = pc.Map[String, JsonValue])
  =>
    _data = data'

  fun apply(key: String): JsonValue ? =>
    """Look up a value by key. Raises if key is not present."""
    _data(key)?

  fun get_or_else(key: String, default: JsonValue): JsonValue =>
    """Look up a value by key, returning default if absent."""
    _data.get_or_else(key, default)

  fun contains(key: String): Bool =>
    """Check whether a key is present."""
    _data.contains(key)

  fun size(): USize =>
    """Number of key-value pairs."""
    _data.size()

  fun update(key: String, value: JsonValue): JsonObject =>
    """Return a new object with the key set to value."""
    JsonObject(_data(key) = value)

  fun remove(key: String): JsonObject =>
    """Return a new object without the given key. No-op if key is absent."""
    JsonObject(_data.sub(key))

  fun keys(): pc.MapKeys[String, JsonValue, col.HashEq[String]] =>
    """Iterate over keys."""
    _data.keys()

  fun values(): pc.MapValues[String, JsonValue, col.HashEq[String]] =>
    """Iterate over values."""
    _data.values()

  fun pairs(): pc.MapPairs[String, JsonValue, col.HashEq[String]] =>
    """Iterate over (key, value) pairs."""
    _data.pairs()

  fun string(): String iso^ =>
    """Compact JSON serialization."""
    _JsonPrint.compact_object(this)

  fun pretty_string(indent: String = "  "): String iso^ =>
    """Pretty-printed JSON serialization."""
    _JsonPrint.pretty_object(this, indent)
