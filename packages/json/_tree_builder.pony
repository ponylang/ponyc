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

class ref _TreeBuilder is JsonTokenNotify
  """
  Internal token consumer that assembles a JsonValue tree from token events.
  Used by JsonParser to build the full parse result.
  """

  var _stack: Array[(_ObjectInProgress | _ArrayInProgress)]
  var _result: (JsonValue | _NoResult)

  new ref create() =>
    _stack = Array[(_ObjectInProgress | _ArrayInProgress)]
    _result = _NoResult

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    match \exhaustive\ token
    | JsonTokenObjectStart =>
      _stack.push(_ObjectInProgress)
    | JsonTokenArrayStart =>
      _stack.push(_ArrayInProgress)
    | JsonTokenKey =>
      try
        match _stack(_stack.size() - 1)?
        | let obj: _ObjectInProgress =>
          obj.pending_key = parser.last_string
        end
      end
    | JsonTokenString =>
      _add_value(parser.last_string)
    | JsonTokenNumber =>
      match \exhaustive\ parser.last_number
      | let n: I64 => _add_value(n)
      | let n: F64 => _add_value(n)
      end
    | JsonTokenTrue =>
      _add_value(true)
    | JsonTokenFalse =>
      _add_value(false)
    | JsonTokenNull =>
      _add_value(None)
    | JsonTokenObjectEnd =>
      try
        let obj = _stack.pop()? as _ObjectInProgress
        _add_value(JsonObject(obj.map))
      end
    | JsonTokenArrayEnd =>
      try
        let arr = _stack.pop()? as _ArrayInProgress
        _add_value(JsonArray(arr.vec))
      end
    end

  fun ref _add_value(value: JsonValue) =>
    if _stack.size() == 0 then
      _result = value
    else
      try
        match \exhaustive\ _stack(_stack.size() - 1)?
        | let obj: _ObjectInProgress =>
          match obj.pending_key
          | let key: String =>
            obj.map = obj.map(key) = value
            obj.pending_key = None
          end
        | let arr: _ArrayInProgress =>
          arr.vec = arr.vec.push(value)
        end
      end
    end

  fun result(): (JsonValue | _NoResult) =>
    _result
