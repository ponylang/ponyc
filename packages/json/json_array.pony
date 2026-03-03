use pc = "collections/persistent"

class val JsonArray is Stringable
  """
  Immutable JSON array backed by a persistent vector.

  Construction is via chained `.push()` calls, each returning a new array
  with structural sharing:

  ```pony
  let arr = JsonArray
    .push(I64(1))
    .push(I64(2))
    .push(I64(3))
  ```
  """

  let _data: pc.Vec[JsonValue]

  new val create(data': pc.Vec[JsonValue] = pc.Vec[JsonValue]) =>
    _data = data'

  fun apply(i: USize): JsonValue ? =>
    """Look up a value by index. Raises if out of bounds."""
    _data(i)?

  fun size(): USize =>
    """Number of elements."""
    _data.size()

  fun update(i: USize, value: JsonValue): JsonArray ? =>
    """
    Return a new array with element at index i replaced.
    Raises if out of bounds.
    """
    JsonArray(_data(i)? = value)

  fun push(value: JsonValue): JsonArray =>
    """Return a new array with value appended."""
    JsonArray(_data.push(value))

  fun pop(): (JsonArray, JsonValue) ? =>
    """
    Return (new array without last element, removed element).
    Raises if empty.
    """
    let last = _data(_data.size() - 1)?
    (JsonArray(_data.pop()?), last)

  fun values(): pc.VecValues[JsonValue] =>
    """Iterate over values."""
    _data.values()

  fun pairs(): pc.VecPairs[JsonValue] =>
    """Iterate over (index, value) pairs."""
    _data.pairs()

  fun string(): String iso^ =>
    """Compact JSON serialization."""
    _JsonPrint.compact_array(this)

  fun pretty_string(indent: String = "  "): String iso^ =>
    """Pretty-printed JSON serialization."""
    _JsonPrint.pretty_array(this, indent)
