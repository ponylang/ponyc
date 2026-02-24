class val JsonNav
  """
  Chained navigation wrapper for JSON values.

  Wraps a JsonValue and provides safe chained access where JsonNotFound propagates
  through the chain — no exceptions until you extract a typed terminal value.

  ```pony
  let nav = JsonNav(json)
  try
    let name = nav("user")("name").as_string()?
    let age = nav("user")("age").as_i64()?
  end
  ```
  """

  let _value: (JsonValue | JsonNotFound)

  new val create(value: JsonValue) =>
    """Wrap a JSON value for navigation."""
    _value = value

  new val _from(value: (JsonValue | JsonNotFound)) =>
    """Internal: wrap a value that may already be JsonNotFound."""
    _value = value

  fun apply(key_or_index: (String | USize)): JsonNav =>
    """
    Navigate into an object by key or array by index.

    If the current value is JsonNotFound, the wrong type, or the key/index
    is missing, returns a JsonNotFound-wrapping nav. JsonNotFound propagates
    through subsequent navigations.
    """
    match (_value, key_or_index)
    | (let obj: JsonObject, let key: String) =>
      try JsonNav._from(obj(key)?)
      else JsonNav._from(JsonNotFound)
      end
    | (let arr: JsonArray, let idx: USize) =>
      try JsonNav._from(arr(idx)?)
      else JsonNav._from(JsonNotFound)
      end
    else
      JsonNav._from(JsonNotFound)
    end

  // --- Terminal extractors ---

  fun as_string(): String ? =>
    """Extract as String. Raises if not a string or JsonNotFound."""
    _value as String

  fun as_i64(): I64 ? =>
    """Extract as I64. Raises if not an integer or JsonNotFound."""
    _value as I64

  fun as_f64(): F64 ? =>
    """Extract as F64. Raises if not a float or JsonNotFound."""
    _value as F64

  fun as_bool(): Bool ? =>
    """Extract as Bool. Raises if not a boolean or JsonNotFound."""
    _value as Bool

  fun as_null(): None ? =>
    """Extract as None (JSON null). Raises if not null or JsonNotFound."""
    _value as None

  fun as_object(): JsonObject ? =>
    """Extract as JsonObject. Raises if not an object or JsonNotFound."""
    _value as JsonObject

  fun as_array(): JsonArray ? =>
    """Extract as JsonArray. Raises if not an array or JsonNotFound."""
    _value as JsonArray

  // --- Inspection ---

  fun json(): (JsonValue | JsonNotFound) =>
    """Get the raw value for pattern matching."""
    _value

  fun found(): Bool =>
    """Check whether navigation succeeded (value is not JsonNotFound)."""
    _value isnt JsonNotFound

  fun size(): USize ? =>
    """Size of the wrapped collection. Raises if not an object or array."""
    match _value
    | let obj: JsonObject => obj.size()
    | let arr: JsonArray => arr.size()
    else error
    end
