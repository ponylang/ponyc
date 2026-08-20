use col = "collections"
use pc = "collections/persistent"

class val JSONObject
  """
  Immutable JSON object backed by a persistent hash map.

  Construction is via chained `.update()` calls, each returning a new object
  with structural sharing:

  ```pony
  let obj = JSONObject
    .update("name", "Alice")
    .update("age", I64(30))
  ```
  """

  let _data: pc.Map[String, JSONValue]

  new val create(
    data': pc.Map[String, JSONValue] = pc.Map[String, JSONValue])
  =>
    _data = data'

  fun apply(key: String): JSONValue ? =>
    """
    Look up a value by key. Raises if key is not present.
    """
    _data(key)?

  fun get_or_else(key: String, default: JSONValue): JSONValue =>
    """
    Look up a value by key, returning default if absent.
    """
    _data.get_or_else(key, default)

  fun contains(key: String): Bool =>
    """
    Check whether a key is present.
    """
    _data.contains(key)

  fun size(): USize =>
    """
    Number of key-value pairs.
    """
    _data.size()

  fun update(key: String, value: JSONValue): JSONObject =>
    """
    Return a new object with the key set to value.
    """
    JSONObject(_data(key) = value)

  fun remove(key: String): JSONObject =>
    """
    Return a new object without the given key. No-op if key is absent.
    """
    JSONObject(_data.sub(key))

  fun keys(): pc.MapKeys[String, JSONValue, col.HashEq[String]] =>
    """
    Iterate over keys.
    """
    _data.keys()

  fun values(): pc.MapValues[String, JSONValue, col.HashEq[String]] =>
    """
    Iterate over values.
    """
    _data.values()

  fun pairs(): pc.MapPairs[String, JSONValue, col.HashEq[String]] =>
    """
    Iterate over (key, value) pairs.
    """
    _data.pairs()

  fun print(): String iso^ =>
    """
    Compact JSON serialization.
    """
    _JSONPrint.compact_object(this)

  fun pretty_print(indent: String = "  "): String iso^ =>
    """
    Pretty-printed JSON serialization.
    """
    _JSONPrint.pretty_object(this, indent)
