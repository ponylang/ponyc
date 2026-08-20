class val JSONNav
  """
  Chained navigation wrapper for JSON values.

  Wraps a JSONValue and provides safe chained access where
  JSONNotFound propagates
  through the chain — no exceptions until you extract a typed terminal value.

  ```pony
  let nav = JSONNav(json)
  try
    let name = nav("user")("name").as_string()?
    let age = nav("user")("age").as_i64()?
  end
  ```
  """

  let _value: (JSONValue | JSONNotFound)

  new val create(value: JSONValue) =>
    """
    Wrap a JSON value for navigation.
    """
    _value = value

  new val _from(value: (JSONValue | JSONNotFound)) =>
    """
    Internal: wrap a value that may already be JSONNotFound.
    """
    _value = value

  fun apply(key_or_index: (String | USize)): JSONNav =>
    """
    Navigate into an object by key or array by index.

    If the current value is JSONNotFound, the wrong type, or the key/index
    is missing, returns a JSONNotFound-wrapping nav. JSONNotFound propagates
    through subsequent navigations.
    """
    match (_value, key_or_index)
    | (let obj: JSONObject, let key: String) =>
      try JSONNav._from(obj(key)?)
      else JSONNav._from(JSONNotFound)
      end
    | (let arr: JSONArray, let idx: USize) =>
      try JSONNav._from(arr(idx)?)
      else JSONNav._from(JSONNotFound)
      end
    else
      JSONNav._from(JSONNotFound)
    end

  // --- Terminal extractors ---
  fun as_string(): String ? =>
    """
    Extract as String. Raises if not a string or JSONNotFound.
    """
    _value as String

  fun as_i64(): I64 ? =>
    """
    Extract as I64. Raises if not an integer or JSONNotFound.
    """
    _value as I64

  fun as_f64(): F64 ? =>
    """
    Extract as F64. Raises if not a float or JSONNotFound.
    """
    _value as F64

  fun as_bool(): Bool ? =>
    """
    Extract as Bool. Raises if not a boolean or JSONNotFound.
    """
    _value as Bool

  fun as_null(): None ? =>
    """
    Extract as None (JSON null). Raises if not null or JSONNotFound.
    """
    _value as None

  fun as_object(): JSONObject ? =>
    """
    Extract as JSONObject. Raises if not an object or JSONNotFound.
    """
    _value as JSONObject

  fun as_array(): JSONArray ? =>
    """
    Extract as JSONArray. Raises if not an array or JSONNotFound.
    """
    _value as JSONArray

  // --- Inspection ---
  fun json(): (JSONValue | JSONNotFound) =>
    """
    Get the raw value for pattern matching.
    """
    _value

  fun found(): Bool =>
    """
    Check whether navigation succeeded (value is not JSONNotFound).
    """
    _value isnt JSONNotFound

  fun size(): USize ? =>
    """
    Size of the wrapped collection. Raises if not an object or array.
    """
    match _value
    | let obj: JSONObject => obj.size()
    | let arr: JSONArray => arr.size()
    else error
    end
