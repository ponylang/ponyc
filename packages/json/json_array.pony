use pc = "collections/persistent"

class val JSONArray
  """
  Immutable JSON array backed by a persistent vector.

  Construction is via chained `.push()` calls, each returning a new array
  with structural sharing:

  ```pony
  let arr = JSONArray
    .push(I64(1))
    .push(I64(2))
    .push(I64(3))
  ```
  """

  let _data: pc.Vec[JSONValue]

  new val create(data': pc.Vec[JSONValue] = pc.Vec[JSONValue]) =>
    _data = data'

  fun apply(i: USize): JSONValue ? =>
    """
    Look up a value by index. Raises if out of bounds.
    """
    _data(i)?

  fun size(): USize =>
    """
    Number of elements.
    """
    _data.size()

  fun update(i: USize, value: JSONValue): JSONArray ? =>
    """
    Return a new array with element at index i replaced.
    Raises if out of bounds.
    """
    JSONArray(_data(i)? = value)

  fun push(value: JSONValue): JSONArray =>
    """
    Return a new array with value appended.
    """
    JSONArray(_data.push(value))

  fun pop(): (JSONArray, JSONValue) ? =>
    """
    Return (new array without last element, removed element).
    Raises if empty.
    """
    let last = _data(_data.size() - 1)?
    (JSONArray(_data.pop()?), last)

  fun values(): pc.VecValues[JSONValue] =>
    """
    Iterate over values.
    """
    _data.values()

  fun pairs(): pc.VecPairs[JSONValue] =>
    """
    Iterate over (index, value) pairs.
    """
    _data.pairs()

  fun print(): String iso^ =>
    """
    Compact JSON serialization.
    """
    _JSONPrint.compact_array(this)

  fun pretty_print(indent: String = "  "): String iso^ =>
    """
    Pretty-printed JSON serialization.
    """
    _JSONPrint.pretty_array(this, indent)
