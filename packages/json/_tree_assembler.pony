use pc = "collections/persistent"

primitive _NoResult

class ref _ObjectInProgress
  var map: pc.Map[String, JsonValue]
  var pending_key: (String | None)

  new ref create() =>
    map = pc.Map[String, JsonValue]
    pending_key = None

class ref _ArrayInProgress
  var vec: pc.Vec[JsonValue]

  new ref create() =>
    vec = pc.Vec[JsonValue]

class ref _TreeAssembler
  """
  Folds a JsonValue tree from a sequence of structural and scalar events.

  Shared by _TreeBuilder (which drives it from the batch JsonTokenParser's
  token callback) and JsonStreamParser (which drives it directly as it scans).
  It knows nothing about tokens, parsers, or readers — only how to assemble
  objects, arrays, and scalars into a `JsonValue`. This is the single place the
  tree shape is built, so the batch and streaming parsers cannot drift in how
  they assemble results.
  """
  embed _stack: Array[(_ObjectInProgress | _ArrayInProgress)]
  var _result: (JsonValue | _NoResult)

  new ref create() =>
    _stack = Array[(_ObjectInProgress | _ArrayInProgress)]
    _result = _NoResult

  fun ref begin_object() =>
    """Open a new object; subsequent key/value pairs fold into it."""
    _stack.push(_ObjectInProgress)

  fun ref begin_array() =>
    """Open a new array; subsequent values fold into it."""
    _stack.push(_ArrayInProgress)

  fun ref key(name: String) =>
    """Set the pending key for the next value added to the current object."""
    try
      match _stack(_stack.size() - 1)?
      | let obj: _ObjectInProgress => obj.pending_key = name
      else _Unreachable() // key() is only called with an object on top
      end
    else _Unreachable()
    end

  fun ref value(v: JsonValue) =>
    """Add a completed scalar (or nested) value at the current position."""
    _add_value(v)

  fun ref end_object() =>
    """Close the current object and fold it into its parent (or the result)."""
    try
      let obj = _stack.pop()? as _ObjectInProgress
      _add_value(JsonObject(obj.map))
    else _Unreachable()
    end

  fun ref end_array() =>
    """Close the current array and fold it into its parent (or the result)."""
    try
      let arr = _stack.pop()? as _ArrayInProgress
      _add_value(JsonArray(arr.vec))
    else _Unreachable()
    end

  fun result(): (JsonValue | _NoResult) =>
    """The assembled top-level value, or _NoResult if none completed yet."""
    _result

  fun ref reset() =>
    """Clear all state to assemble a fresh top-level value."""
    _stack.clear()
    _result = _NoResult

  fun ref _add_value(v: JsonValue) =>
    if _stack.size() == 0 then
      _result = v
    else
      try
        match \exhaustive\ _stack(_stack.size() - 1)?
        | let obj: _ObjectInProgress =>
          match obj.pending_key
          | let k: String =>
            obj.map = obj.map(k) = v
            obj.pending_key = None
          end
        | let arr: _ArrayInProgress =>
          arr.vec = arr.vec.push(v)
        end
      else _Unreachable()
      end
    end
